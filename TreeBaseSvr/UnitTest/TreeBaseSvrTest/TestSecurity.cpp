#include "stdafx.h"
#if defined(UNIT_TEST)
#define MOCK_SEGMENT
#endif
#include "defs.h"
#include "..\..\TUniqueBinarySegment.h"
#include "..\..\TSecurityAttributes.h"
#include <vector>
#include "MockTask.h"

#define TBACCESS_READ    0x1
#define TBACCESS_WRITE   0x2
#define TBACCESS_RW      0x3
#define TBACCESS_EXECUTE 0x4
#define TBACCESS_ALL     0x7

//======================================================================
template<typename T>
T* blockInit(int a_nSize)
{
    int *pArr = (int*)TASK_MemAlloc(a_nSize * sizeof(int));

    for(int i = 0; i < a_nSize; i++)
    {
        pArr[i] = i;
    }
    return pArr;
}

//======================================================================
template<typename T>
void memCompare(T* a_pArr, T* a_pArr2)
{
    TEST_VERIFY( TASK_MemSize(a_pArr) == TASK_MemSize(a_pArr2) );

    TEST_VERIFY( memcmp(a_pArr, a_pArr2, TASK_MemSize(a_pArr)) == 0 );
}

//======================================================================
bool operator==(
      const std::vector<Security::BW_LIST_ENTRY> & _lhs
    , const std::vector<Security::BW_LIST_ENTRY> & _rhs )
{
    if(_lhs.size() != _rhs.size())
        return false;

    for(int i = 0; i < _lhs.size(); ++i)
    {
        if(_lhs[i].user != _rhs[i].user)
            return false;
        if(_lhs[i].accessRights != _rhs[i].accessRights)
            return false;
    }
    return true;
}

//***********************************************************************
TEST_ENTRY(Security_UniqueBinarySegment, "Unique Binary Segment")
{
    TUniqueBinarySegment ubs;
    ubs.Resize(1);

    UNIQUE_BINARY_REF ref;
    //----------------------------------------------------------------
    {
        TTaskAutoPtr<int> pArr(blockInit<int>(10));
        ref = ubs.AddBinaryBlock(pArr);

        TTaskAutoPtr<int> pArr2((int*)ubs.GetBinaryBlock(ref));

        memCompare<int>(pArr, pArr2);

        UNIQUE_BINARY_REF ref2 = ubs.AddBinaryBlock(pArr);
        TEST_VERIFY(ref.dwHash == ref2.dwHash && ref.wOrder == ref2.wOrder);

        //ubs.DeleteBinaryBlock(ref);
        ubs.DeleteBinaryBlock(ref2);
    }
    //----------------------------------------------------------------
    {
        TTaskAutoPtr<int> pArr(blockInit<int>(25));
        
        ubs.ChangeBinaryBlock(ref, pArr);
        
        TTaskAutoPtr<int> pArr2((int*)ubs.GetBinaryBlock(ref));

        memCompare<int>(pArr, pArr2);
    }
    //----------------------------------------------------------------
    {
        TTaskAutoPtr<int> pArr(blockInit<int>(225));
        
        ubs.ChangeBinaryBlock(ref, pArr);
        
        TTaskAutoPtr<int> pArr2((int*)ubs.GetBinaryBlock(ref));

        memCompare<int>(pArr, pArr2);
    }
    return true;
}

//***********************************************************************
class BlackWhiteListInit
{
    std::vector<Security::BW_LIST_ENTRY> m_list;
public:
    BlackWhiteListInit(const Util::Uuid &a_user, int a_rights)
    {
        Security::BW_LIST_ENTRY entry = {a_user, a_rights};
        m_list.push_back(entry);
    }
    BlackWhiteListInit & operator()(const Util::Uuid &a_user, int a_rights)
    {
        Security::BW_LIST_ENTRY entry = {a_user, a_rights};
        m_list.push_back(entry);
        return *this;
    }

    operator std::vector<Security::BW_LIST_ENTRY>()
    {
        return m_list;
    }
};

//***********************************************************************
template<bool _ExpectedResult> 
bool isGrantedEqual(int a_granted, int a_expectedGranted)
{
    return a_granted == a_expectedGranted;
}

//***********************************************************************
template<> 
bool isGrantedEqual<false>(int a_granted, int a_expectedGranted)
{
    return true;
}

//***********************************************************************
template<bool _ExpectedResult, int _ExpectedGranted>
void testAccessCheck(
    const Security::TSecurityAttributes &a_secAttrs,
    const Security::TTaskSecurityContext *a_pContext, 
    int a_minimalDesired, 
    int a_maximalDesired)
{
    // Original
    int granted;
    bool result = a_secAttrs.accessCheck(a_pContext, a_minimalDesired, a_maximalDesired, granted);

    TEST_VERIFY(isGrantedEqual<_ExpectedResult>(granted, _ExpectedGranted));
    TEST_VERIFY(result == _ExpectedResult);

    // Converted to block
    Security::SECURITY_ATTRIBUTES_BLOCK *pBlock = a_secAttrs.getBlock();
    result = a_secAttrs.accessCheck(a_pContext, a_minimalDesired, a_maximalDesired, granted);    

    TEST_VERIFY(isGrantedEqual<_ExpectedResult>(granted, _ExpectedGranted));
    TEST_VERIFY(result == _ExpectedResult);

    int cbSize = TASK_MemSize(pBlock);
    Security::SECURITY_ATTRIBUTES_BLOCK *pBlock2 = (Security::SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(cbSize);
    memcpy(pBlock2, pBlock, cbSize);

    // Other converted to editable again
    Security::TSecurityAttributes attrs2(pBlock2);
    attrs2.getBlackList();
    result = a_secAttrs.accessCheck(a_pContext, a_minimalDesired, a_maximalDesired, granted);    

    TEST_VERIFY(isGrantedEqual<_ExpectedResult>(granted, _ExpectedGranted));
    TEST_VERIFY(result == _ExpectedResult);
}

//***********************************************************************
TEST_ENTRY(Security_SecurityAttributes, "SecurityAttributes")
{
    Security::TTaskSecurityContext context;
    
    context.privilege = Security::TTaskSecurityContext::PrivilegeLevel::eSystem;
    context.user = Util::UuidGenerate();

    //------------------------------------------------------------------------
    // System privilege
    {
        Security::TSecurityAttributes secAttrs(TBACCESS_READ, TBACCESS_RW, TBACCESS_ALL );
        secAttrs.getBase()->owner = context.user;
        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_RW);
    }
    //------------------------------------------------------------------------
    // Owner
    {
        context.privilege = Security::TTaskSecurityContext::PrivilegeLevel::eUser;

        Security::TSecurityAttributes secAttrs(TBACCESS_READ, TBACCESS_RW, TBACCESS_ALL );
        secAttrs.getBase()->owner = context.user;
        testAccessCheck<true, TBACCESS_ALL>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);
    }
    //------------------------------------------------------------------------
    // Any user
    {
        Security::TSecurityAttributes secAttrs(TBACCESS_READ, TBACCESS_RW, TBACCESS_ALL );
        secAttrs.getBase()->owner = Util::UuidGenerate();
        testAccessCheck<false, 0>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_READ>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_ALL);
    }
    //------------------------------------------------------------------------
    // Administrative privilege
    {
        context.privilege = Security::TTaskSecurityContext::PrivilegeLevel::eAdmin;

        Security::TSecurityAttributes secAttrs(TBACCESS_READ, TBACCESS_RW, TBACCESS_ALL);
        secAttrs.getBase()->owner = Util::UuidGenerate();
        
        testAccessCheck<false, 0>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_RW, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_READ>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_READ);

        secAttrs.getBase()->owner = context.user;
        testAccessCheck<true, TBACCESS_ALL>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);
    }
    //------------------------------------------------------------------------
    // With black list
    {
        Util::Uuid uid = Util::UuidGenerate();
        context.privilege = Security::TTaskSecurityContext::PrivilegeLevel::eUser;
        context.user = uid;

        Security::TSecurityAttributes secAttrs(TBACCESS_ALL, 0, 0);

        secAttrs.getBlackList() = BlackWhiteListInit
            (Util::UuidGenerate(), TBACCESS_ALL)
            (uid, TBACCESS_EXECUTE);

        testAccessCheck<false, 0>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_RW, TBACCESS_ALL);

        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_ALL);

        context.user = Util::UuidGenerate();
        testAccessCheck<true, TBACCESS_ALL>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);
    }
    //------------------------------------------------------------------------
    // With white list
    {
        Util::Uuid uidOwner = Util::UuidGenerate();
        Util::Uuid uidAllowed = Util::UuidGenerate();
        Util::Uuid uid = Util::UuidGenerate();
        context.user = uid;

        Security::TSecurityAttributes secAttrs(0, 0, TBACCESS_ALL);
        secAttrs.getBase()->owner = uidOwner;

        secAttrs.getWhiteList() = BlackWhiteListInit
            (uidAllowed, TBACCESS_ALL)
            (Util::UuidGenerate(), TBACCESS_EXECUTE);

        testAccessCheck<false, 0>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);
        testAccessCheck<false, 0>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_READ);

        context.user = uidAllowed;
        testAccessCheck<true, TBACCESS_ALL>(
            secAttrs, &context, TBACCESS_ALL, TBACCESS_ALL);
        testAccessCheck<true, TBACCESS_RW>(
            secAttrs, &context, TBACCESS_READ, TBACCESS_RW);
    }
    //------------------------------------------------------------------------
    return true;
}

//***********************************************************************
TEST_ENTRY(Security_SecurityAttributeOperations, "SecurityAttributeOperations")
{
    Util::Uuid user1 = Util::UuidGenerate();
    Util::Uuid user2 = Util::UuidGenerate();
    Util::Uuid user3 = Util::UuidGenerate();
    Util::Uuid user4 = Util::UuidGenerate();

    //------------------------------------------------------------------------
    // Add operations
    {
        Util::Uuid user5 = Util::UuidGenerate();

        Security::TSecurityAttributes secAttrs(0, 0, TBACCESS_ALL);

        secAttrs.getWhiteList() = BlackWhiteListInit
            (user1, TBACCESS_ALL)
            (user2, TBACCESS_EXECUTE)
            (user3, TBACCESS_READ);

        Security::TSecurityAttributes secAttrsPlus(0, TBACCESS_READ, 0);

        secAttrsPlus.getWhiteList() = BlackWhiteListInit
            (user3, TBACCESS_WRITE)
            (user4, TBACCESS_READ);

        secAttrsPlus.getBlackList() = BlackWhiteListInit
            (user5, TBACCESS_EXECUTE);

        secAttrs += secAttrsPlus;

        TEST_VERIFY( secAttrs.getBase()->ownerAccessRights == TBACCESS_ALL );
        TEST_VERIFY( secAttrs.getBase()->adminAccessRights == TBACCESS_READ );
        TEST_VERIFY( secAttrs.getBase()->allAccessRights == 0 );

        TEST_VERIFY( secAttrs.getWhiteList() == BlackWhiteListInit
            (user1, TBACCESS_ALL)
            (user2, TBACCESS_EXECUTE)
            (user3, TBACCESS_RW)
            (user4, TBACCESS_READ)
        );

        TEST_VERIFY( secAttrs.getBlackList() == BlackWhiteListInit
            (user5, TBACCESS_EXECUTE)
        );
    }
    //------------------------------------------------------------------------
    // Substract operations
    {
        Security::TSecurityAttributes secAttrs(TBACCESS_READ, TBACCESS_RW, TBACCESS_ALL);
        
        secAttrs.getWhiteList() = BlackWhiteListInit
            (user1, TBACCESS_ALL)
            (user2, TBACCESS_EXECUTE)
            (user3, TBACCESS_RW);
 
        Security::TSecurityAttributes secAttrsMinus(TBACCESS_READ, TBACCESS_WRITE, 0);

        secAttrsMinus.getWhiteList() = BlackWhiteListInit
            (user2, TBACCESS_EXECUTE)
            (user3, TBACCESS_WRITE);

        secAttrs -= secAttrsMinus;
 
        TEST_VERIFY( secAttrs.getBase()->ownerAccessRights == TBACCESS_ALL );
        TEST_VERIFY( secAttrs.getBase()->adminAccessRights == TBACCESS_READ );
        TEST_VERIFY( secAttrs.getBase()->allAccessRights == 0 );

        TEST_VERIFY( secAttrs.getWhiteList() == BlackWhiteListInit
            (user1, TBACCESS_ALL)
            (user3, TBACCESS_READ)
        );
   }
    return true;
}