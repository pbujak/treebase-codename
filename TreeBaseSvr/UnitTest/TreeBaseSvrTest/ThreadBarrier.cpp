#include "StdAfx.h"
#include "ThreadBarrier.h"

CThreadBarrier::CThreadBarrier(int a_nThreads): m_nSize(a_nThreads), m_nIndex(0)
{
    m_pEvents = new HANDLE[a_nThreads];

    for(int i = 0; i < a_nThreads; i++)
    {
        m_pEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
}

void CThreadBarrier::wait()
{
    HANDLE hEvent = m_pEvents[m_nIndex];
    InterlockedIncrement(&m_nIndex);

    SetEvent(hEvent);
    WaitForMultipleObjects(m_nSize, m_pEvents, TRUE, INFINITE);
    ResetEvent(hEvent);
    InterlockedDecrement(&m_nIndex);
}

CThreadBarrier::~CThreadBarrier(void)
{
    for(int i = 0; i < m_nSize; i++)
    {
        CloseHandle(m_pEvents[i]);
    }
    delete m_pEvents;
}
