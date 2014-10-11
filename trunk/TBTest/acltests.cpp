#include "stdafx.h"
#include "TestFramework.h"
#include "TBaseObj.h"
#include "TreeBase.h"
#include <atlsecurity.h>
#include <afxtempl.h>
#include "TAbstractThreadLocal.h"

typedef CMap<ATL::CSid, const ATL::CSid&, DWORD, DWORD> MapSidToRights;

static TThreadLocal<MapSidToRights> s_mapSidToRights;

//*********************************************************************************
template<>
UINT HashKey<const ATL::CSid&>(const ATL::CSid & a_sid)
{
    BYTE *pSid = (BYTE*)a_sid.GetPSID();

    UINT hash = 0;
    int nOffset = 0;

    int nSize = GetLengthSid(pSid);

    for(int i = 0; i < nSize; ++i)
    {
        hash |= (pSid[i] << nOffset);

        nOffset = (nOffset + 8) % 32;
    }
    return hash;
}

//*********************************************************************************
static void setRights(CTBAcl &a_acl, const ATL::CSid &a_sid, DWORD a_dwAccessRights)
{
    a_acl.SetRights(a_sid, a_dwAccessRights);

    MapSidToRights &mapSidToRights = s_mapSidToRights;
    mapSidToRights[a_sid] = a_dwAccessRights;
}

//*********************************************************************************
static void resetRights(CTBAcl &a_acl, const ATL::CSid &a_sid)
{
    a_acl.ResetRights(a_sid);

    s_mapSidToRights.Value().RemoveKey(a_sid);
}

//*********************************************************************************
static void verifyAccessControlList(CTBAcl &a_acl)
{
    MapSidToRights &mapSidToRights = s_mapSidToRights;

    int nCount = a_acl.GetCount();
    TEST_VERIFY(nCount == mapSidToRights.GetCount());

    for(int i = 0; i < nCount; ++i)
    {
        ATL::CSid sid;
        DWORD dwAccessRights, dwAccessRights2;
        a_acl.GetEntry(i, sid, dwAccessRights);

        TEST_VERIFY(mapSidToRights.Lookup(sid, dwAccessRights2) && dwAccessRights == dwAccessRights2);
    }
}


//*********************************************************************************
static void doAccessControlListTest()
{
//_xxx_test();
    CTBAcl acl;

    setRights(acl, ATL::CSid("user"), TBACCESS_READ);
    setRights(acl, ATL::CSid("admin"), TBACCESS_ALL);

    setRights(acl, ATL::CSid("NT AUTHORITY\\LocalService"), TBACCESS_BROWSE);
    setRights(acl, ATL::CSid("NT AUTHORITY\\NetworkService"), TBACCESS_BROWSE);

    try
    {
        acl.SetRights(Sids::Admins(), TBACCESS_BROWSE);
        ::testing::fail();
    }
    catch(CTBException *e)
    {
    }

    //----------------------------------------------------------------------------------------------
    verifyAccessControlList(acl);

    //----------------------------------------------------------------------------------------------
    setRights(acl, ATL::CSid("NT AUTHORITY\\NetworkService"), TBACCESS_READ);
    verifyAccessControlList(acl);

    //----------------------------------------------------------------------------------------------
    resetRights(acl, ATL::CSid("user"));
    verifyAccessControlList(acl);
}


DEFINE_TEST_CASE(AccessControlList, 1, doAccessControlListTest)

