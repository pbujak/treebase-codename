#pragma once

class CThreadBarrier
{
    inline CThreadBarrier(void) {};

    HANDLE *m_pEvents;
    int     m_nSize;
    long    m_nIndex;
public:
    CThreadBarrier(int a_nThreads);
    void wait();
public:
    ~CThreadBarrier(void);
};
