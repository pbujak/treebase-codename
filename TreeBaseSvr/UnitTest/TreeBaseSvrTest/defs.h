#ifndef _DEFS_H_
#define _DEFS_H_

#include <cassert>

typedef bool (*PFN_TEST_ENTRY)();

class TTestCase
{
public:
    static TTestCase *s_pHead;

    const char     *m_pszName, *m_pszDesc;
    PFN_TEST_ENTRY  m_pfnEntry;
    TTestCase      *m_pNext;

public:
    inline TTestCase(const char* a_pszName, const char* a_pszDesc, PFN_TEST_ENTRY a_pfnEntry): 
        m_pszDesc(a_pszDesc), m_pszName(a_pszName), m_pfnEntry(a_pfnEntry)
    {
        m_pNext = s_pHead;
        s_pHead = this;
    }
    bool runTest()
    {
        return m_pfnEntry();
    }
};

#define TEST_VERIFY(x) if(!(x)) { DebugBreak(); throw std::exception(); }

#define TEST_ENTRY(name, desc) \
    static bool TestProc_##name(); \
    static TTestCase TestObj_##name(#name, #desc, &TestProc_##name); \
    static bool TestProc_##name()

#endif // _DEFS_H_