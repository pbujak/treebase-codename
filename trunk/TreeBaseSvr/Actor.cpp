#include "stdafx.h"
#include "Actor.h"

using namespace Actor;

TMessageQueueTask::TMessageQueueTask(int a_nTimerRate): 
    m_bWaitForMessage(false), 
    m_nTimerRate(a_nTimerRate),
    m_bStop(false)
{
    m_messageQueue_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

TMessageQueueTask::~TMessageQueueTask(void)
{
}

//*****************************************************************
HANDLE TMessageQueueTask::WaitForObjects(TCSLock &a_lock)
{
    int                 nCount(m_mapExtraHandles.size() + 1);
    std::vector<HANDLE> handles;
    
    handles.reserve(nCount);
    handles.push_back(m_messageQueue_hEvent);

    typedef std::map<HANDLE, TAbstractMessageFactory*>::const_iterator ItMap;
    for(ItMap it = m_mapExtraHandles.begin(); it != m_mapExtraHandles.end(); it++)
    {
        handles.push_back((*it).first);
    }
    //----------------------------------------------
    m_bWaitForMessage = true;
    a_lock.unlock();
    DWORD dwRes = WaitForMultipleObjects(nCount, &handles[0], FALSE, m_nTimerRate);
    a_lock.lock();
    m_bWaitForMessage = false;

    //----------------------------------------------
    if(dwRes >= WAIT_OBJECT_0 && dwRes < (WAIT_OBJECT_0 + nCount))
    {
        return handles[dwRes - WAIT_OBJECT_0];
    }
    return NULL;
}

//*****************************************************************
TAbstractMessage* TMessageQueueTask::GetMessage(int a_nTimeout)
{
    TCSLock _lock(&m_messageQueue_CS);

    while(m_messageQueue.empty())
    {
        HANDLE handle = WaitForObjects(_lock);

        if(handle == NULL)
            return NULL;

        typedef std::map<HANDLE, TAbstractMessageFactory*>::const_iterator ItMap;
        ItMap it = m_mapExtraHandles.find(handle);
        if(it != m_mapExtraHandles.end())
        {
            TAbstractMessageFactory *pMsgFactory = m_mapExtraHandles[handle];
            TAbstractMessage *pMsg = pMsgFactory ? pMsgFactory->create() : NULL;
            return pMsg;
        }
    }
    TAbstractMessage* pMsg = m_messageQueue.front();
    m_messageQueue.pop();
    return pMsg;
}

//*****************************************************************
void TMessageQueueTask::Send(TAbstractMessage* a_pMsg)
{
    TCSLock _lock(&m_messageQueue_CS);

    m_messageQueue.push(a_pMsg);
    if(m_bWaitForMessage)
    {
        SetEvent(m_messageQueue_hEvent);
    }
}


//*****************************************************************
void TMessageQueueTask::OnRun()
{
    while(!m_bStop)
    {
        TAbstractMessage *pMsg = GetMessage(m_nTimerRate);

        if(pMsg)
        {
            pMsg->handle();
            delete pMsg;
        }
        else
        {
            DWORD dwTicks = GetTickCount();
            OnTimer(dwTicks);
        }
    }
}
