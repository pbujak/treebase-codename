#include "stdafx.h"
#include "SecurityManager.h"
#include "TSecurityAttributes.h"
#include "TUniqueBinarySegment.h"
#include "TSegmentSecurityCache.h"
#include "SegmentMgr.h"
#include "Shared\TAbstractThreadLocal.h"

using namespace Security;

namespace
{
    struct SecInfo
    {
        UNIQUE_BINARY_REF ref;
        SECURITY_ATTRIBUTES_BLOCK* pBlock;
    };
};

static TThreadLocal<TTaskSecurityContext> s_taskContext;

//================================================================================
SECURITY_BASE_INFORMATION Security::Manager::AdminSecurity  = { Util::EmptyUuid, Util::EmptyUuid, TBACCESS_READ, TBACCESS_ALL, 0 };
SECURITY_BASE_INFORMATION Security::Manager::SystemSecurity = { Util::EmptyUuid, Util::EmptyUuid, TBACCESS_READ, 0, 0 };
SECURITY_BASE_INFORMATION Security::Manager::TempSecurity   = { Util::EmptyUuid, Util::EmptyUuid, TBACCESS_ALL, 0, 0 };
SECURITY_BASE_INFORMATION Security::Manager::LabelSecurity  = { Util::EmptyUuid, Util::EmptyUuid, TBACCESS_ALL, 0, 0 };
SECURITY_BASE_INFORMATION Security::Manager::BlobSecurity   = { Util::EmptyUuid, Util::EmptyUuid, TBACCESS_ALL, TBACCESS_ALL, TBACCESS_ALL };
SECURITY_BASE_INFORMATION Security::Manager::AuthSecurity   = { Util::EmptyUuid, Util::EmptyUuid, 0, TBACCESS_ALL, 0 };

//***********************************************************************
void Security::Manager::InheritSecurity(TSecurityAttributes &a_attributes)
{
    TTaskSecurityContext &context = s_taskContext.Value();
    Security::SECURITY_BASE_INFORMATION *pSecBase = a_attributes.getBase();

    if(pSecBase->owner != context.user && pSecBase->owner != Util::EmptyUuid) // owner changed
    {
        for(const BW_LIST_ENTRY *pEntry = a_attributes.beginWhiteList(); pEntry != a_attributes.endWhiteList(); ++pEntry)
        {
            if(pEntry->user == pSecBase->owner)
            {
                const_cast<BW_LIST_ENTRY *>(pEntry)->accessRights |= pSecBase->ownerAccessRights;
                return;
            }
        }

        BW_LIST_ENTRY prevOwnerRights = {pSecBase->owner, pSecBase->ownerAccessRights};
        a_attributes.getWhiteList().push_back(prevOwnerRights);
    }
}

//***********************************************************************
bool Security::Manager::ValidateNewSecurity(const TSecurityAttributes &a_attributes)
{
    if(!a_attributes.getOwnerOverride())
        a_attributes.getBase()->owner = s_taskContext.Value().user;

    if(s_taskContext.Value().privilege != TTaskSecurityContext::PrivilegeLevel::eSystem)
    {
        SECURITY_BASE_INFORMATION *pBase = a_attributes.getBase();
        int rights = pBase->allAccessRights | pBase->adminAccessRights;

        if((rights & TBACCESS_ADMIN_MINIMAL) != TBACCESS_ADMIN_MINIMAL)
            return false;
    }

    int granted;
    return a_attributes.accessCheck(&s_taskContext.Value(), TBACCESS_READ, TBACCESS_READ, granted);
}

//***********************************************************************
REF Security::Manager::AddSecurity(int a_ndbId, const TSecurityAttributes &a_attributes)
{
    Storage::TAdvPageRef xPageRef(a_ndbId);

    xPageRef.lockGuardPageForWrite((FPOINTER)1);
    TSystemSegment<TUniqueBinarySegment, SEGID_SECDATA> securitySegment(a_ndbId);

    return securitySegment.AddBinaryBlock(a_attributes.getBlock());
}

//***********************************************************************
void Security::Manager::DeleteSecurity(int a_ndbId, const REF& a_refSecurity)
{
    Storage::TAdvPageRef xPageRef(a_ndbId);

    xPageRef.lockGuardPageForWrite((FPOINTER)1);
    TSystemSegment<TUniqueBinarySegment, SEGID_SECDATA> securitySegment(a_ndbId);

    securitySegment.DeleteBinaryBlock(a_refSecurity);
}

//***********************************************************************
bool Security::Manager::GetSecurity(int a_ndbId, const REF& a_refSecurity, TSecurityAttributes &a_attributes)
{
    Storage::TAdvPageRef xPageRef(a_ndbId);

    xPageRef.lockGuardPageForRead((FPOINTER)1);
    TSystemSegment<TUniqueBinarySegment, SEGID_SECDATA> securitySegment(a_ndbId);

    SECURITY_ATTRIBUTES_BLOCK *pBlock = (SECURITY_ATTRIBUTES_BLOCK *)securitySegment.GetBinaryBlock(a_refSecurity);

    if(pBlock == NULL)
        return false;

    a_attributes.attach(pBlock);
    return true;
}

//***********************************************************************
bool Security::Manager::SetSecurity(int a_ndbId, 
                                    const Util::Uuid& a_protectionDomain, 
                                    TBSECURITY_OPERATION::Enum operation, 
                                    TSecurityAttributes &a_attributes)
{
    Storage::TAdvPageRef xPageRef(a_ndbId);

    xPageRef.lockGuardPageForWrite((FPOINTER)1);
    TSystemSegment<TUniqueBinarySegment, SEGID_SECDATA> securitySegment(a_ndbId);

    TSectionSegment::Iterator iterator;

    TSectionSegment::Ref<TBITEM_ENTRY> itemP = securitySegment.GetFirstItem(iterator, xPageRef);

    std::vector<SecInfo> secIds;

    while(itemP)
    {
        TSectionSegment::Ref<SECURITY_ATTRIBUTES_BLOCK> secP = securitySegment.GetDataOfEntryRef<SECURITY_ATTRIBUTES_BLOCK>(itemP);
        if(secP->base.protectionDomain == a_protectionDomain)
        {
            SecInfo secInfo;
            secInfo.ref.dwHash = itemP->dwHash;
            secInfo.ref.wOrder = itemP->wOrder;

            int cbSize = securitySegment.GetDataOfEntrySize(itemP);
            secInfo.pBlock = (SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(cbSize);
            memcpy(secInfo.pBlock, (void*)secP, cbSize);

            secIds.push_back(secInfo);
        }
        itemP = securitySegment.GetNextItem(iterator);
    }

    for(std::vector<SecInfo>::iterator it = secIds.begin(); it != secIds.end(); ++it)
    {
        Security::TSecurityAttributes secAttrs(it->pBlock);
        it->pBlock = NULL;

        MergeSecurity(operation, secAttrs, a_attributes);
        if(!securitySegment.ChangeBinaryBlock(it->ref, secAttrs.getBlock()))
            return false;
    }

    Security::TSegmentSecurityCache::getInstance().removeSecurity(a_protectionDomain);

    return true;
}

//***********************************************************************
bool Security::Manager::AccessCheck(int a_ndbId, const REF& a_refSecurity, 
                                    int a_minimalDesired, int a_maximalDesired, int &a_granted)
{
    TSecurityAttributes attributes;

    if(!Security::Manager::GetSecurity(a_ndbId, a_refSecurity, attributes))
        return false;

    return attributes.accessCheck(&s_taskContext.Value(), a_minimalDesired, a_maximalDesired, a_granted);
}

//***********************************************************************
TTaskSecurityContext* Security::Manager::GetTaskContext()
{
    return &s_taskContext.Value();
}

//***********************************************************************
void Security::Manager::MergeSecurity(TBSECURITY_OPERATION::Enum operation, 
                                      TSecurityAttributes &a_target,
                                      const TSecurityAttributes &a_source)
{
    switch(operation)
    {
        case TBSECURITY_OPERATION::eREPLACE:
        {
            Util::Uuid owner = a_target.getBase()->owner;
            Util::Uuid protectionDomain = a_target.getBase()->protectionDomain;

            a_target = a_source;
            a_target.getBase()->owner = owner;
            a_target.getBase()->protectionDomain = protectionDomain;

            break;
        }
        case TBSECURITY_OPERATION::eADD:
        {
            a_target += a_source;
            break;
        }
        case TBSECURITY_OPERATION::eREMOVE:
        {
            a_target -= a_source;
            break;
        }
    }
}

//***********************************************************************
Security::TPrivilegedScope::TPrivilegedScope()
{
    Security::TTaskSecurityContext* pContext = Manager::GetTaskContext();
    m_savedContext = *pContext;
    pContext->privilege = Security::TTaskSecurityContext::PrivilegeLevel::eSystem;
    pContext->user = Util::EmptyUuid;
}

//***********************************************************************
Security::TPrivilegedScope::~TPrivilegedScope()
{
    Security::TTaskSecurityContext* pContext = Manager::GetTaskContext();
    *pContext = m_savedContext;
}