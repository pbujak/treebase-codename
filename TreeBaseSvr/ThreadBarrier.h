#pragma once

class TThreadBarrier
{
    inline TThreadBarrier(void) {};

    HANDLE *m_pEvents;
    int     m_nSize;
    long    m_nIndex;
public:
    TThreadBarrier(int a_nThreads);
    void wait();
public:
    ~TThreadBarrier(void);
};
