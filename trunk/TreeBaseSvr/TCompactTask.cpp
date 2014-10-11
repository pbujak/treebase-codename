#include "stdafx.h"
#include "TCompactTask.h"
#include "TBaseFile.h"
#include "compact.h"

namespace
{
    //====================================================
    class TCompactImpl: public TCompactBase
    {
        friend class TCompactTask;
        TCompactTask *m_pTask;
    protected:
        inline TCompactImpl(TCompactTask *a_pTask): m_pTask(a_pTask)
        {}

        virtual BOOL IsOverTime();
        virtual void YieldControl();
    };

    //********************************************************
    BOOL TCompactImpl::IsOverTime()
    {
        return TRUE;
    }

    //********************************************************
    void TCompactImpl::YieldControl()
    {
        G_dbManager.UnlockExclusive();
        if(m_pTask->Stopped())
            throw std::exception();

        SwitchToThread();
        G_dbManager.LockExclusive();
    }

} // namespace

//********************************************************
TCompactTask::TCompactTask(void): m_barrier(2), m_bStop(false)
{
}

//********************************************************
TCompactTask::~TCompactTask(void)
{
}

//********************************************************
void TCompactTask::OnRun()
{
    try
    {
        int nIdx = 0;
        while(!m_bStop)
        {
            Sleep(2000);

            int nCount = G_dbManager.GetCount();
            if(nCount)
            {
                Storage::TStorage *pStorage = G_dbManager.GetFile(nIdx);

                if(pStorage && (pStorage->getFlags() & Storage::eFlag_NeedCompact))
                {
                    TCompactImpl compact(this);
                    G_dbManager.LockExclusive();
                    compact.Compact(nIdx);
                    pStorage->setFlags(0, Storage::eFlag_NeedCompact);
                    G_dbManager.UnlockExclusive();
                }
                nIdx++;
                nIdx %= nCount;
            }
        };
    }
    catch(std::exception&)
    {
    }
    m_barrier.wait();
}
