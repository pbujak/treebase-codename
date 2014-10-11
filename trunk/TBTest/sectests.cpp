#include "stdafx.h"
#include "TestFramework.h"
#include "TBaseObj.h"
#include "TreeBase.h"
#include <atlsecurity.h>
#include <exception>
#include <iostream>
#include <string>
#include <cstring>

//*********************************************************************************
bool operator==(const CTBAcl &lhs, const CTBAcl &rhs)
{
    int nCount = lhs.GetCount();

    if(nCount != rhs.GetCount())
        return false;

    for(int i = 0; i < nCount; ++i)
    {
        ATL::CSid sid;
        DWORD rights;

        lhs.GetEntry(i, sid, rights);

        if(rhs.GetRights(sid) != rights)
            return false;
    }

    return true;
}

//*********************************************************************************
bool operator==(const CTBSecurityAttributes &lhs, const CTBSecurityAttributes &rhs)
{
    if((lhs.m_securityAttributes.dwAccessRightsAdmins != rhs.m_securityAttributes.dwAccessRightsAdmins) ||
       (lhs.m_securityAttributes.dwAccessRightsAll !=  rhs.m_securityAttributes.dwAccessRightsAll) ||
       (lhs.m_securityAttributes.dwAccessRightsOwner !=  rhs.m_securityAttributes.dwAccessRightsOwner))
       return false;

    return lhs.m_blackList == rhs.m_blackList && lhs.m_whiteList == rhs.m_whiteList;
}

//*********************************************************************************
static void testProtectionDomain()
{
    CTBSection xCurrentUser;

    xCurrentUser.Open(&CTBSection::tbRootSection, "\\CurrentUser");

    {
        CTBSection xSection1, xSection2;

        xSection1.Create(&xCurrentUser, "Section1", TBATTR_NOVALUES, NULL);
        xSection2.Create(&xCurrentUser, "Section2", TBATTR_NOVALUES, NULL);

        xSection1.Close();
        xSection2.Close();
    }

    GUID domain1, domain2, domain3;

    xCurrentUser.GetSectionSecurity("Section1", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domain1, NULL);
    xCurrentUser.GetSectionSecurity("Section2", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domain2, NULL);

    CTBSection::tbRootSection.GetSectionSecurity("CurrentUser", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domain3, NULL);

    TEST_VERIFY(domain1 == domain2);
    TEST_VERIFY(domain2 == domain3);

    //-----------------------------------------------------------------------
    {
        CTBSecurityAttributes secAttrs(TBACCESS_ALL, TBACCESS_ADMIN_MINIMAL, TBACCESS_READ);
        CTBSection xSection;
        
        xSection.Create(&xCurrentUser, "Section3", TBATTR_NOVALUES, &secAttrs);
        xSection.Close();

        GUID domain2, domain3;

        xCurrentUser.GetSectionSecurity("Section2", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domain2, NULL);
        xCurrentUser.GetSectionSecurity("Section3", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domain3, NULL);

        TEST_VERIFY(domain2 != domain3);
    }

    xCurrentUser.DeleteSection("Section1");
    xCurrentUser.DeleteSection("Section2");
    xCurrentUser.DeleteSection("Section3");
}

//*********************************************************************************
static void testCreateSecionWithSecurity()
{
    CTBSecurityAttributes secAttrs(TBACCESS_ALL, TBACCESS_ALL, TBACCESS_READ);
    secAttrs.m_whiteList.SetRights(ATL::CSid("User"), TBACCESS_ALL);
    secAttrs.m_blackList.SetRights(ATL::CSid("SystemDeveloper"), TBACCESS_ALL);
    secAttrs.m_blackList.SetRights(ATL::CSid("NT AUTHORITY\\LocalService"), TBACCESS_MODIFY);

    CTBSection xDatabases;
    xDatabases.Open(&CTBSection::tbRootSection, "\\Databases");

    CTBSection xSecTestDB;
    xSecTestDB.Create(&xDatabases, "SecTestDB", TBATTR_NOVALUES, &secAttrs);
    xSecTestDB.Close();

    {
        CTBSecurityAttributes secAttrs2;
        GUID protectionDomain;
        ATL::CSid sidOwner, sidUser;

        xDatabases.GetSectionSecurity( 
            "SecTestDB",
            TBSECURITY_INFORMATION::eOWNER | TBSECURITY_INFORMATION::ePROTECTION_DOMAIN | TBSECURITY_INFORMATION::eATTRIBUTES,
            &sidOwner,
            &protectionDomain,
            &secAttrs2
        );

        ATL::CAccessToken token;
        
        token.GetThreadToken(TOKEN_QUERY);

        token.GetUser(&sidUser);
                
        TEST_VERIFY(sidUser == sidOwner);
        TEST_VERIFY(secAttrs2 == secAttrs);

        xDatabases.DeleteSection("SecTestDB");
    }
}

//*********************************************************************************
static void testSecurityOfSystemObjects()
{
    CTBSecurityAttributes systemSecAttrs(0, 0, TBACCESS_READ), adminSecAttrs(0, TBACCESS_ALL, TBACCESS_READ);

    CTBSecurityAttributes secAttrs;
    GUID protectionDomain, protectionDomain2;
    ATL::CSid sidOwner, sidUser;

    CTBSection::tbRootSection.GetSectionSecurity( 
        "System",
        TBSECURITY_INFORMATION::eOWNER | TBSECURITY_INFORMATION::eATTRIBUTES,
        &sidOwner,
        NULL,
        &secAttrs
    );

    TEST_VERIFY(sidOwner == ATL::Sids::Null());
    TEST_VERIFY(secAttrs == systemSecAttrs);

    //----------------------------------------------------------------------
    CTBSection xRootStorage;
    xRootStorage.Open(&CTBSection::tbRootSection, "\\RootStorage");
    
    xRootStorage.GetSectionSecurity( 
        "System",
        TBSECURITY_INFORMATION::eOWNER | TBSECURITY_INFORMATION::ePROTECTION_DOMAIN | TBSECURITY_INFORMATION::eATTRIBUTES,
        &sidOwner,
        &protectionDomain,
        &secAttrs
    );

    TEST_VERIFY(sidOwner == ATL::Sids::Null());
    TEST_VERIFY(secAttrs == adminSecAttrs);

    //----------------------------------------------------------------------
    CTBSection xSystem;
    xSystem.Open(&CTBSection::tbRootSection, "\\RootStorage\\System");
    
    xSystem.GetSectionSecurity( 
        "Authentication",
        TBSECURITY_INFORMATION::eOWNER | TBSECURITY_INFORMATION::ePROTECTION_DOMAIN | TBSECURITY_INFORMATION::eATTRIBUTES,
        &sidOwner,
        &protectionDomain2,
        &secAttrs
    );

    TEST_VERIFY(sidOwner == ATL::Sids::Null());
    TEST_VERIFY(secAttrs == adminSecAttrs);
    TEST_VERIFY(protectionDomain == protectionDomain2);
}

//*********************************************************************************
static void testSetSecurityForSection()
{
    CTBSecurityAttributes secAttrs(TBACCESS_ALL, TBACCESS_ALL, 0);
    secAttrs.m_whiteList.SetRights(ATL::CSid("User"), TBACCESS_ALL);

    CTBSection xDatabases;
    xDatabases.Open(&CTBSection::tbRootSection, "\\Databases");

    //---------------------------------------------------------------------------
    {
        CTBSection xSecTest;
        xSecTest.Create(&xDatabases, "SecTest", 0, &secAttrs);
        //-----------------------------------------------------------------------
        {
            CTBSection xSection1, xSection2;
            xSection1.Create(&xSecTest, "Section1", 0, NULL);
            xSection2.Create(&xSecTest, "Section2", 0, NULL);
        }
        //-----------------------------------------------------------------------
        CTBSecurityAttributes secAttrsDiff(0, 0, 0);
        secAttrsDiff.m_whiteList.SetRights(ATL::CSid("User"), TBACCESS_DELETE);

        //-----------------------------------------------------------------------
        xSecTest.SetSectionSecurity(
            "Section1", 
            TBSECURITY_TARGET::eSECTION,
            TBSECURITY_OPERATION::eREMOVE,
            &secAttrsDiff);

        //-----------------------------------------------------------------------
        GUID domainSecTest = {0};
        GUID domainSection1 = {0};
        xDatabases.GetSectionSecurity("SecTest", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domainSecTest, NULL);
        xSecTest.GetSectionSecurity("Section1", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domainSection1, NULL);

        TEST_VERIFY(domainSecTest != domainSection1);

        //-----------------------------------------------------------------------
        xSecTest.SetSectionSecurity(
            "Section1", 
            TBSECURITY_TARGET::eSECTION,
            TBSECURITY_OPERATION::eADD,
            &secAttrsDiff);

        //-----------------------------------------------------------------------
        xDatabases.GetSectionSecurity("SecTest", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domainSecTest, NULL);
        xSecTest.GetSectionSecurity("Section1", TBSECURITY_INFORMATION::ePROTECTION_DOMAIN, NULL, &domainSection1, NULL);

        TEST_VERIFY(domainSecTest == domainSection1);
    }
    //---------------------------------------------------------------------------
    // clean up
    CTBSection xSecTest;
    xSecTest.Open(&xDatabases, "SecTest");

    xSecTest.DeleteSection("Section1");
    xSecTest.DeleteSection("Section2");
    xSecTest.Close();

    xDatabases.DeleteSection("SecTest");
}

//*********************************************************************************
static void testSetSecurityForDomain()
{
    CTBSecurityAttributes secAttrs(TBACCESS_ALL, TBACCESS_ALL, 0);
    secAttrs.m_whiteList.SetRights(ATL::CSid("User"), TBACCESS_ALL);

    CTBSection xDatabases;
    xDatabases.Open(&CTBSection::tbRootSection, "\\Databases");

    //-------------------------------------------------------------------------
    {
        CTBSection xDomainTest;
        xDomainTest.Create(&xDatabases, "DomainTest", 0, &secAttrs);
        //-------------------------------------------------------------------------
        {
            CTBSection xSection1, xSection2;
            xSection1.Create(&xDomainTest, "Section1", 0, NULL);
            xSection2.Create(&xDomainTest, "Section2", 0, NULL);
        }
        //-------------------------------------------------------------------------
        secAttrs.m_securityAttributes.dwAccessRightsAll = TBACCESS_READ;

        try
        {
            xDatabases.SetSectionSecurity(
                "DomainTest", 
                TBSECURITY_TARGET::ePROTECTION_DOMAIN,
                TBSECURITY_OPERATION::eREPLACE,
                &secAttrs);

            ::testing::fail("Cannot change security of open sections");
        }
        catch(CTBException *e)
        {
            if(e->m_errInfo.dwErrorCode != TRERR_SHARING_DENIED &&
               strcmp(e->m_errInfo.szErrorItem, "DomainTest" ) != 0 &&
               strcmp(e->m_errInfo.szErrorSection, "\\Databases\\" ) != 0)
            {
                ::testing::fail("Invalid error information");
            }
            e->Delete();
        }
    }
    //-------------------------------------------------------------------------
    xDatabases.SetSectionSecurity(
        "DomainTest", 
        TBSECURITY_TARGET::ePROTECTION_DOMAIN,
        TBSECURITY_OPERATION::eREPLACE,
        &secAttrs);

    //-------------------------------------------------------------------------
    // clean up
    CTBSection xDomainTest;
    xDomainTest.Open(&xDatabases, "DomainTest");

    xDomainTest.DeleteSection("Section1");
    xDomainTest.DeleteSection("Section2");
    xDomainTest.Close();

    xDatabases.DeleteSection("DomainTest");
}

//*********************************************************************************
static void doSecurityTest()
{
    BOOL isAdmin;

    TEST_VERIFY(CheckTokenMembership(NULL, (PSID)ATL::Sids::Admins().GetPSID(), &isAdmin));

    //----------------------------------------------------------------------
    if(!isAdmin)
    {
        char szComputer[MAX_PATH] = {0};
        DWORD dwSize = sizeof(szComputer) - 1;
        GetComputerName(szComputer, &dwSize);

        std::string user, passwd;

        std::cout << "Administrator Account:";
        std::getline(std::cin, user);

        std::cout << "Password:";
        std::getline(std::cin, passwd);

        ATL::CAccessToken token;

        if(!token.LogonUser(user.c_str(), szComputer, passwd.c_str(), LOGON32_LOGON_INTERACTIVE))
        {
            std::cout << '\n' << "Logon error " << GetLastError() << '\n';
            ::testing::fail();
        }

        TEST_VERIFY(token.ImpersonateLoggedOnUser());

        token.GetThreadToken(TOKEN_QUERY);
        TEST_VERIFY(token.CheckTokenMembership(ATL::Sids::Admins(), (bool*)&isAdmin));

        if(!isAdmin)
            ::testing::fail("Not administrator account");
    }
    //----------------------------------------------------------------------
    testCreateSecionWithSecurity();
    testSecurityOfSystemObjects();
    testProtectionDomain();
    testSetSecurityForDomain();
    testSetSecurityForSection();
}


DEFINE_TEST_CASE(Security, 1, doSecurityTest)
