#include "stdafx.h"
#include "session.h"
#include "ReferenceCountedMap.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TBaseFile.h"
#include "section.h"
#include "TSystemStatus.h"
#include "SecurityFacade.h"
#include "TSecurityAttributes.h"
#include <atlsecurity.h>

using namespace Session;

namespace
{

TCriticalSection                                      s_cs;
Util::CReferenceCountedMap<CString, LPCSTR, UserInfo> s_userMap;
Util::CReferenceCountedMap<CString, LPCSTR, SEGID_F>  s_computerMap;

//=====================================================================================
struct ISectionFactory
{
    virtual TSection* createSection(TSection *a_pParent, LPCSTR a_pszName) const = 0;
};

//=====================================================================================
class UserSectionFactory: public ISectionFactory
{
    CSid m_sysUID;
    Util::Uuid m_uid;
public:
    UserSectionFactory(ATL::CSid &a_sid, Util::Uuid &a_uid):
        m_sysUID(a_sid), m_uid(a_uid)
    {}

    virtual TSection* createSection(TSection *a_pParent, LPCSTR a_pszName) const;
};

//=====================================================================================
class ComputerSectionFactory: public ISectionFactory
{
public:
    ComputerSectionFactory() {}

    virtual TSection* createSection(TSection *a_pParent, LPCSTR a_pszName) const;
};

//********************************************************************
TSection* UserSectionFactory::createSection(TSection *a_pParent, LPCSTR a_pszName) const
{
    Security::TSecurityAttributes secAttrs(TBACCESS_BROWSE | TBACCESS_GETVALUE, TBACCESS_ALL, TBACCESS_ALL);

    secAttrs.setOwnerOverride(true);
    secAttrs.getBase()->owner = m_uid;

    CString strName = m_sysUID.Domain() + CString(".") + m_sysUID.AccountName();

    Util::TSectionPtr refSection = SECTION_CreateSection(a_pParent, strName, TBATTR_NOVALUES, &secAttrs);
    if(refSection == NULL)
        return NULL;

    refSection = NULL;

    a_pParent->CreateLink(a_pParent, strName, a_pszName);

    return SECTION_OpenSection(a_pParent, a_pszName, TBOPEN_MODE_DYNAMIC);
}

//********************************************************************
TSection* ComputerSectionFactory::createSection(TSection *a_pParent, LPCSTR a_pszName) const
{
    return SECTION_CreateSection(a_pParent, a_pszName, TBATTR_NOVALUES, NULL);
}

} // namespace


//********************************************************************
static SEGID_F getJunctionPoint(LPCSTR a_pszBase, LPCSTR a_pszName, const ISectionFactory &a_factory)
{
    CString strFullPath;
    CString strPath = a_pszBase;
    strPath += "\\";
    strPath += a_pszName;

    SEGID_F segId = TSection::ResolvePath(NULL, strPath, strFullPath);

    if(segId != SEGID_NULL)
        return segId;

    Util::TSectionPtr parent = SECTION_OpenSection(NULL, a_pszBase, TBOPEN_MODE_DYNAMIC);

    if(parent == NULL)
    {
        CString strError;
        strError.Format("Cannot open section\"%s\"", a_pszBase);
        System::setAlarm("SESSION", strError);
        return SEGID_NULL;
    }

    Util::TSectionPtr section = a_factory.createSection(parent, a_pszName);

    if(section == NULL)
    {
        CString strError;
        strError.Format("Cannot create section\"%s\"", strPath);
        System::setAlarm("SESSION", strError);
        return SEGID_NULL;
    }

    segId.ndbId = section->m_ndbId;
    segId.nSegId = section->m_nSegId;
    return segId;
}

//********************************************************************
void Session::getUserInfo(ATL::CSid &a_sid, UserInfo &a_info)
{
    CString strID = a_sid.Sid();

    TCSLock _lock(&s_cs);

    if(s_userMap.getValue(strID, a_info))
        return;

    a_info.uid = Security::Facade::getInternalUID(a_sid);
    if(a_info.uid == Security::Facade::EmergencyUID)
    {
        a_info.junctionSegId = SEGID_NULL;
    }
    else
        a_info.junctionSegId = getJunctionPoint("\\RootStorage\\Users", Util::UuidToSectionName(a_info.uid), UserSectionFactory(a_sid, a_info.uid));

    s_userMap.addValue(strID, a_info);
}

//********************************************************************
void Session::releaseUserInfo(ATL::CSid &a_sid)
{
    TCSLock _lock(&s_cs);
    s_userMap.releaseValue(a_sid.Sid());
}

//********************************************************************
void Session::releaseComputerInfo(LPCSTR a_pszComputer)
{
    TCSLock _lock(&s_cs);
    s_computerMap.releaseValue(a_pszComputer);
}


//********************************************************************
void Session::getComputerInfo(LPCSTR a_pszComputer, SEGID_F &a_info)
{
    TCSLock _lock(&s_cs);

    if(s_computerMap.getValue(a_pszComputer, a_info))
        return;

    a_info = getJunctionPoint("\\RootStorage\\Computers", a_pszComputer, ComputerSectionFactory());

    s_computerMap.addValue(a_pszComputer, a_info);
}
