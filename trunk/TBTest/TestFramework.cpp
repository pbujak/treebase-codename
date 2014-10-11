#include "stdafx.h"
#include "TestFramework.h"
#include "TAbstractThreadLocal.h"
#include <stdio.h>
#include <string>

TTestCaseInit *TTestCaseInit::s_pFirst = NULL;

//*********************************************************************************
void TTestCaseInit::getTestCases(std::vector<TEST_CASE*> &a_vecTestCases)
{
    TTestCaseInit *pTestCase = s_pFirst;

    while(pTestCase)
    {
        a_vecTestCases.push_back(pTestCase->m_pTestCase);
        pTestCase = pTestCase->m_pNext;
    }
}

//================================================================================
namespace
{

//===================================================================================
class TFailException
{
public:
    TFailException(const std::string& message) : m_message(message)
    {}
    const char* message()
    {
        return m_message.c_str();
    }
private:
    std::string m_message;
};

//===================================================================================
class TThreadBarrier
{
    CRITICAL_SECTION m_cs;
    HANDLE  m_hSem, m_hNoWaiters;
    int     m_nCount;
    volatile int     m_nWaiters;
    int     m_nStopped;

public:
    TThreadBarrier(int a_nCount);
    ~TThreadBarrier();
    void notifyEndThread(int a_nThread);
    void wait(int a_nThread);
};

//===================================================================================
class TCSLock
{
    CRITICAL_SECTION *m_pcs;
public:
    inline TCSLock(CRITICAL_SECTION *a_pcs): m_pcs(a_pcs)
    {
        EnterCriticalSection(a_pcs);
    }
    inline ~TCSLock()
    {
        LeaveCriticalSection(m_pcs);
    }
    inline void unlock()
    {
        LeaveCriticalSection(m_pcs);
    }
    inline void lock()
    {
        EnterCriticalSection(m_pcs);
    }
};

//===================================================================================
class TCurrentTest
{
public:
    //********************************************************************
    inline TCurrentTest(int a_nThreads): 
        m_barrier(a_nThreads), 
        m_nCounter(a_nThreads), 
        m_hStopEvent(CreateEvent(NULL, TRUE, FALSE, NULL)),
        m_bResult(true)
    {
        InitializeCriticalSection(&m_cs);
    };

    //********************************************************************
    inline ~TCurrentTest()
    {
        LeaveCriticalSection(&m_cs);
        CloseHandle(m_hStopEvent);
    }

    //********************************************************************
    void reportStopped()
    {
        TCSLock csLock(&m_cs);

        if(--m_nCounter == 0)
            SetEvent(m_hStopEvent);
    }
    //********************************************************************
    void setResult(bool a_bResult)
    {
        TCSLock csLock(&m_cs);

        m_bResult &= a_bResult;
    }

    //------------------------------------------------------------------
    HANDLE           m_hStopEvent;
    CRITICAL_SECTION m_cs;
    TThreadBarrier m_barrier;
    int            m_nCounter;
    bool           m_bResult;
    TEST_CASE     *m_pTestCase;
};

struct STestParams
{
    int m_nThread;
    TCurrentTest *m_pTest;
};

//=================================================================================
TThreadLocal<int>            t_nThread;
TThreadLocal<TCurrentTest*>  t_pCurrentTest;

//*********************************************************************************
TThreadBarrier::TThreadBarrier(int a_nCount): 
    m_nCount(a_nCount), m_nWaiters(0), m_nStopped(0)
{
    InitializeCriticalSection(&m_cs);
    m_hSem = CreateSemaphore(NULL, 0, 10, NULL);
    m_hNoWaiters = CreateEvent(NULL, TRUE, TRUE, NULL);
}

//*********************************************************************************
void TThreadBarrier::wait(int a_nThread)
{
    EnterCriticalSection(&m_cs);
    m_nWaiters++;

    if((m_nWaiters + m_nStopped) == m_nCount)
    {
        ReleaseSemaphore(m_hSem, m_nWaiters-1, NULL);
        ResetEvent(m_hNoWaiters);
    }
    else
    {
        LeaveCriticalSection(&m_cs);
        WaitForSingleObject(m_hSem, INFINITE);
        EnterCriticalSection(&m_cs);
    }
    if(--m_nWaiters == 0)
        SetEvent(m_hNoWaiters);
    LeaveCriticalSection(&m_cs);

    WaitForSingleObject(m_hNoWaiters, INFINITE);
}

//*********************************************************************************
void TThreadBarrier::notifyEndThread(int a_nThread)
{
    TCSLock lock(&m_cs);

    if(m_nWaiters) 
        ReleaseSemaphore(m_hSem, 1, NULL);

    m_nStopped++;
}

//*********************************************************************************
TThreadBarrier::~TThreadBarrier()
{
    CloseHandle(m_hSem);
    CloseHandle(m_hNoWaiters);
    DeleteCriticalSection(&m_cs);
}

//*********************************************************************************
UINT testEntry(void* a_pParam)
{
    STestParams *pParams = (STestParams *)a_pParam;

    t_nThread = pParams->m_nThread;
    t_pCurrentTest = pParams->m_pTest;

    try
    {
        (*pParams->m_pTest->m_pTestCase->pfnEntry)();
    }
    catch(TFailException &e)
    {
        std::cout << "\n" << e.message() << "\n\n";
        pParams->m_pTest->setResult(false);
    }
    catch(CException *e)
    {
        char buff[256] = {0};
        e->GetErrorMessage(buff, 256);
        std::cout << "\n" << buff << "\n\n";
        pParams->m_pTest->setResult(false);
    }
    catch(...)
    {
        std::cout << "\nexception occured\n\n";
        pParams->m_pTest->setResult(false);
    }

    pParams->m_pTest->m_barrier.notifyEndThread(pParams->m_nThread);
    pParams->m_pTest->reportStopped();
    delete pParams;
    return 1;
}

//*********************************************************************************
void runOneTest(TEST_CASE *a_pTestCase)
{
    TCurrentTest currentTest(a_pTestCase->nThreads);

    currentTest.m_pTestCase = a_pTestCase;
    
    std::cout << "==================================================================\n";
    std::cout << "     *** TEST CASE: " << a_pTestCase->szName << " *** \n";
    std::cout << "\n";

    for(int i = 0; i < a_pTestCase->nThreads; i++)
    {
        STestParams *pParams = new STestParams();
        pParams->m_nThread = i;
        pParams->m_pTest = &currentTest;

        AfxBeginThread(&testEntry, pParams);
    }

    do
    {
        MSG msg = {0};

        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
        }
    }
    while(MsgWaitForMultipleObjects(1, &currentTest.m_hStopEvent, FALSE, INFINITE, QS_ALLEVENTS) != WAIT_OBJECT_0);

    std::cout << "     " << (currentTest.m_bResult ? "+PASSED" : "-FAILED") << "\n\n";
}

} // namespace

//*********************************************************************************
int testing::getThreadIndex()
{
    return t_nThread;
}

//*********************************************************************************
void testing::threadBarrier()
{
    t_pCurrentTest.Value()->m_barrier.wait(t_nThread);
}

//*********************************************************************************
void testing::fail(const std::string &message)
{
    throw TFailException(message);
}

//*********************************************************************************
void testing::setResult(bool a_bResult)
{
    t_pCurrentTest.Value()->setResult(a_bResult);
}

//*********************************************************************************
void testing::runTest(TEST_CASE *a_pTestCase)
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONIN$", "r", stdin);
    
    if(a_pTestCase)
    {
        runOneTest(a_pTestCase);
    }
    else
    {
        std::vector<TEST_CASE*> vecTestCases;
        TTestCaseInit::getTestCases(vecTestCases);

        for(std::vector<TEST_CASE*>::const_iterator it = vecTestCases.begin(); it != vecTestCases.end(); ++it)
        {
            runOneTest(*it);
        }
    }

    std::cout << "Press Enter to continue...";

    {
        DWORD read;
        INPUT_RECORD ir;
        for(;; )
        {
            ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),&ir,1,&read);
            if (ir.EventType==KEY_EVENT)
                break;
        }
    }

    FreeConsole();
}
