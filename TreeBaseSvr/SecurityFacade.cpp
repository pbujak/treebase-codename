#include "stdafx.h"
#include "SecurityFacade.h"
#include "TSecurityAttributes.h"
#include "SecurityManager.h"
#include "section.h"
#include "Util.h"
#include "TSystemStatus.h"
#include "InternalSection.h"
#include "lpc.h"
#include "lpccodes.h"
#include "TACLUtil.h"

using namespace Security::Facade;

const Util::Uuid Security::Facade::EmergencyUID("D688EA0C-E092-42D8-9436-2897A01B063F");

//***************************************************************************
static void addUserMapping(TSection *a_pSection, ATL::CSid &a_sid, const Util::Uuid &a_uid)
{
    System::TStatusMap valueMap;
    valueMap.SetValue(a_sid.Sid(), Util::UuidToString(a_uid));
    valueMap.SetValue(Util::UuidToSectionName(a_uid), a_sid.Sid());

    LPVOID pvBatch = valueMap.ExportBatch();
    a_pSection->ProcessBatch(pvBatch);
    TASK_MemFree(pvBatch);
}

//***************************************************************************
static ATL::CSid sidFromString(LPCSTR a_pszSid)
{
    PSID psid;
    if(!ConvertStringSidToSid(a_pszSid, &psid))
        return ATL::Sids::Null();

    ATL::CSid sid(*(SID*)psid);
    LocalFree(psid);
    return sid;
}

//***************************************************************************
Util::Uuid Security::Facade::getInternalUID(ATL::CSid &a_sid)
{
    // UID from SID in mapping
    Util::TSectionPtr mappingSection = SECTION_OpenSection(NULL, "\\System\\UserMapping", TBOPEN_MODE_DYNAMIC);
    if(mappingSection)
    {
        Util::Internal::TSectionAccessor accessor(mappingSection);
        TBVALUE_ENTRY* pValue = accessor.getValue(a_sid.Sid(), TBVTYPE_TEXT);
        if(pValue)
            return Util::UuidFromString(pValue->text);
    }

    // From persistence storage
    Util::TSectionPtr authSection = SECTION_OpenSection(NULL, "\\RootStorage\\System\\Authentication", TBOPEN_MODE_DYNAMIC);
    if(authSection == NULL)
    {
        System::setAlarm("SESSION", "Cannot open \\RootStorage\\System\\Authentication");
        return EmergencyUID;
    }

    Util::TSectionPtr sidSection = SECTION_OpenSection(authSection, a_sid.Sid(), TBOPEN_MODE_DYNAMIC);
    if(sidSection)
    {
        Util::Internal::TSectionAccessor accessor(sidSection);
        TBVALUE_ENTRY* pValue = accessor.getValue("UID", TBVTYPE_TEXT);
        if(pValue)
        {
            Util::Uuid uid = Util::UuidFromString(pValue->text);
            addUserMapping(mappingSection, a_sid, uid);
            return uid;
        }
    }
    else
    {
        sidSection = SECTION_CreateSection(authSection, a_sid.Sid(), 0, NULL);
        if(sidSection == NULL)
        {
            System::setAlarm("SESSION", CString("Cannot create \\RootStorage\\System\\Authentication\\") + a_sid.Sid());
            return EmergencyUID;
        }

        System::TStatusMap valueMap;
        
        Util::Uuid uid = Util::UuidGenerate();
        valueMap.SetValue("SID", a_sid.Sid());
        valueMap.SetValue("UID", Util::UuidToString(uid));
        valueMap.SetValue("UserName", a_sid.AccountName());
        valueMap.SetValue("Domain", a_sid.Domain());

        LPVOID pvBatch = valueMap.ExportBatch();
        sidSection->ProcessBatch(pvBatch);
        TASK_MemFree(pvBatch);

        sidSection = NULL;

        authSection->CreateLink(authSection, a_sid.Sid(), Util::UuidToSectionName(uid));

        addUserMapping(mappingSection, a_sid, uid);
        return uid;
    }

    return EmergencyUID;
}

//***************************************************************************
ATL::CSid Security::Facade::getSystemUID(const Util::Uuid &a_uid)
{
    if(a_uid == Util::EmptyUuid)
        return ATL::Sids::Null();

    // SID from UID in mapping
    Util::TSectionPtr mappingSection = SECTION_OpenSection(NULL, "\\System\\UserMapping", TBOPEN_MODE_DYNAMIC);
    if(mappingSection)
    {
        Util::Internal::TSectionAccessor accessor(mappingSection);
        TBVALUE_ENTRY* pValue = accessor.getValue(Util::UuidToSectionName(a_uid), TBVTYPE_TEXT);
        if(pValue)
            return sidFromString(pValue->text);
    }

    // From persistence storage
    Util::TSectionPtr authSection = SECTION_OpenSection(NULL, "\\RootStorage\\System\\Authentication", TBOPEN_MODE_DYNAMIC);
    if(authSection == NULL)
    {
        System::setAlarm("SESSION", "Cannot open \\RootStorage\\System\\Authentication");
        return ATL::CSid();
    }

    Util::TSectionPtr sidSection = SECTION_OpenSection(authSection, Util::UuidToSectionName(a_uid), TBOPEN_MODE_DYNAMIC);
    if(sidSection)
    {
        Util::Internal::TSectionAccessor accessor(sidSection);
        TBVALUE_ENTRY* pValue = accessor.getValue("SID", TBVTYPE_TEXT);
        if(pValue)
        {
            ATL::CSid sid = sidFromString(pValue->text);
            addUserMapping(mappingSection, sid, a_uid);
            return sid;
        }
    }
    else
    {
        System::setAlarm("SECURITY", Util::message("Cannot find system UID for internal %s", Util::UuidToString(a_uid)));
    }

    return ATL::CSid();
}

//***************************************************************************
static bool serializeBlackWhiteList(TCallParams &a_outParams, int a_nCode, const Security::BW_LIST_ENTRY *a_pBegin, const Security::BW_LIST_ENTRY *a_pEnd)
{
    if(a_pBegin == a_pEnd)
        return true;

    Security::TPrivilegedScope privilegedScope;

    TACLSerializer aclSerializer;

    for(const Security::BW_LIST_ENTRY *pEntry = a_pBegin; pEntry != a_pEnd; ++pEntry)
    {
        ATL::CSid sid = getSystemUID(pEntry->user);
        if(!sid.IsValid())
            return false;
        aclSerializer.AddEntry((PSID)sid.GetPSID(), pEntry->accessRights);
    }

    void *pvBuff = aclSerializer.GetResult();
    a_outParams.EatBuffer(a_nCode, pvBuff);
    return true;
}

//***************************************************************************
static bool deserializeBlackWhiteList(TCallParams &a_inParams, int a_nCode, std::vector<Security::BW_LIST_ENTRY>& a_list)
{
    void *pvList = NULL;
    long cbSize = 0;

    if(!a_inParams.GetBuffer(a_nCode, &pvList, cbSize))
        return true;

    Security::TPrivilegedScope privilegedScope;

    ACL_BLOCK_ENTRY *pEntry = (ACL_BLOCK_ENTRY *)pvList;

    while(pEntry->cbSize)
    {
        Security::BW_LIST_ENTRY bwListEntry;

        ATL::CSid sid( *(SID*)pEntry->sid );
        bwListEntry.user = getInternalUID(sid);
        bwListEntry.accessRights = pEntry->dwRights;

        if(bwListEntry.user == EmergencyUID)
        {
            CString userName = sid.Domain() + CString("\\") + sid.AccountName();
            TASK_SetErrorInfo(TRERR_CANNOT_REGISTER_USER, userName);
            return false;
        }
        a_list.push_back(bwListEntry);

        pEntry = MAKE_PTR(ACL_BLOCK_ENTRY*, pEntry, pEntry->cbSize);
    }
    return true;
}

//***************************************************************************
bool Security::Facade::serializeSecurity(TCallParams &a_outParams, std::auto_ptr<TSecurityAttributes> &a_inSecAttrs)
{
    if(a_inSecAttrs.get() == NULL)
        return true;

    Security::SECURITY_BASE_INFORMATION *pBase = a_inSecAttrs->getBase();

    TBSECURITY_ATTRIBUTES secAttrs = {pBase->ownerAccessRights, pBase->adminAccessRights, pBase->allAccessRights};
    a_outParams.SetBuffer(SVP_SECURITY_ATTRIBUTES_BASE, &secAttrs, sizeof(secAttrs));

    if(!serializeBlackWhiteList(a_outParams, SVP_SECURITY_ATTRIBUTES_WHITE_LIST, a_inSecAttrs->beginWhiteList(), a_inSecAttrs->endWhiteList()))
        return false;

    if(!serializeBlackWhiteList(a_outParams, SVP_SECURITY_ATTRIBUTES_BLACK_LIST, a_inSecAttrs->beginBlackList(), a_inSecAttrs->endBlackList()))
        return false;

    return true;
}

//***************************************************************************
bool Security::Facade::deserializeSecurity(TCallParams &a_inParams, std::auto_ptr<TSecurityAttributes> &a_outSecAttrs)
{
    TBSECURITY_ATTRIBUTES *pInAttrs = NULL;
    long cbSize = 0;

    if(!a_inParams.GetBuffer(SVP_SECURITY_ATTRIBUTES_BASE, (void**)&pInAttrs, cbSize))
        return true;

    std::auto_ptr<TSecurityAttributes> secAttrs(new TSecurityAttributes(pInAttrs->dwAccessRightsAll, pInAttrs->dwAccessRightsAdmins, pInAttrs->dwAccessRightsOwner));

    if(!deserializeBlackWhiteList(a_inParams, SVP_SECURITY_ATTRIBUTES_BLACK_LIST, secAttrs->getBlackList()))
        return false;

    if(!deserializeBlackWhiteList(a_inParams, SVP_SECURITY_ATTRIBUTES_WHITE_LIST, secAttrs->getWhiteList()))
        return false;

    a_outSecAttrs = secAttrs;

    return true;
}
