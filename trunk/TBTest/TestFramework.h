#ifndef _TEST_FRAMEWORK_H_
#define _TEST_FRAMEWORK_H_

#include <vector>
#include <iostream>

//====================================================================================
typedef struct _TEST_CASE
{
    const char *szName;
    int         nThreads;

    void (*pfnEntry)();
}TEST_CASE;

//====================================================================================
class TTestCaseInit
{
    TEST_CASE     *m_pTestCase;
    TTestCaseInit *m_pNext;

    static TTestCaseInit *s_pFirst;
public:
    inline TTestCaseInit(TEST_CASE *a_pTestCase): m_pTestCase(a_pTestCase)
    {
        m_pNext = s_pFirst;
        s_pFirst = this;
    }
    static void getTestCases(std::vector<TEST_CASE*> &a_vecTestCases);
};


//====================================================================================
#define DEFINE_TEST_CASE(name, threads, func) \
    namespace name \
    { \
        static TEST_CASE testCase = { #name, threads, func }; \
        static TTestCaseInit testCaseInit(&testCase); \
    }

#define TEST_VERIFY_NOT(x) \
    if (x) \
    { \
        ::testing::fail("\"" #x "\" is TRUE, expected FALSE"); \
    }

#define TEST_VERIFY(x) \
    if (!(x)) \
    { \
        ::testing::fail("\"" #x "\" failed"); \
    }

#define ASSERT_NOTNULL(x) TEST_VERIFY((x) != NULL)


namespace testing
{
    void runTest(TEST_CASE *a_pTestCase);

    int getThreadIndex();

    void threadBarrier();

    void setResult(bool a_bResult);

    void fail(const std::string &message = "");
}

#endif // _TEST_FRAMEWORK_H_
