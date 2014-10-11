#pragma once
#include "ttask.h"
#include "ThreadBarrier.h"

class TCompactTask : public TTask
{
    TThreadBarrier m_barrier;
    bool           m_bStop;
public:
    TCompactTask(void);

    inline bool Stopped()
    { 
        return m_bStop; 
    }
public:
    virtual ~TCompactTask(void);

    virtual void OnRun();

    inline void Stop()
    {
        m_bStop = true;
        m_barrier.wait();
    }
};
