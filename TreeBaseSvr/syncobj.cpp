#include "stdafx.h"
#include "syncobj.h"
#include <assert.h>

#ifdef _WIN32

void RWLock_Initialize(PRWLOCK rwlock)
{
    memset(rwlock, 0, sizeof(RWLOCK));
    InitializeCriticalSection(&rwlock->internalLock);
    InitializeCriticalSection(&rwlock->writerLock);

    /*
     * We use a manual-reset event as poor man condition variable that
     * can only do broadcast.  Actually, only one thread will be waiting
     * on this at any time, because the wait is done while holding the
     * writerLock.
     */
    rwlock->noReaders = CreateEvent (NULL, TRUE, TRUE, NULL);
}

void RWLock_ReadLock(PRWLOCK rwlock)
{
    /*
     * We need to lock the writerLock too, otherwise a writer could
     * do the whole of RWLock_WriteLock after the readerCount changed
     * from 0 to 1, but before the event was reset.
     */
    EnterCriticalSection(&rwlock->writerLock);
    EnterCriticalSection(&rwlock->internalLock);
    ++rwlock->readerCount;
    LeaveCriticalSection(&rwlock->internalLock);
    LeaveCriticalSection(&rwlock->writerLock);
}

void RWLock_WriteLock(PRWLOCK rwlock)
{
    EnterCriticalSection(&rwlock->writerLock);
    EnterCriticalSection(&rwlock->internalLock);
    if (rwlock->readerCount > 0) 
    {
        rwlock->waitNoReaders = true;
        ResetEvent(rwlock->noReaders);
        LeaveCriticalSection(&rwlock->internalLock);
        WaitForSingleObject(rwlock->noReaders, INFINITE);
        EnterCriticalSection(&rwlock->internalLock); 
        rwlock->waitNoReaders = false;
    }
    LeaveCriticalSection(&rwlock->internalLock);
    
    //if (rwlock->readerCount > 0) {
    //    WaitForSingleObject(rwlock->noReaders, INFINITE);
    //}

    /* writerLock remains locked.  */
}

void RWLock_ReadUnlock(PRWLOCK rwlock)
{
    EnterCriticalSection(&rwlock->internalLock);
    assert (rwlock->readerCount > 0);
    if (--rwlock->readerCount == 0) 
    {
        if(rwlock->waitNoReaders)
        {
            SetEvent(rwlock->noReaders);
        }
    }
    LeaveCriticalSection(&rwlock->internalLock);
}

void RWLock_WriteUnlock(PRWLOCK rwlock)
{
    LeaveCriticalSection(&rwlock->writerLock);
}

void RWLock_Delete(PRWLOCK rwlock)
{
    DeleteCriticalSection(&rwlock->internalLock);
    DeleteCriticalSection(&rwlock->writerLock);
    CloseHandle(rwlock->noReaders);
}

#else

void TSemaphore::up(int a_nCount)
{
    for(int ctr = 0; ctr < a_nCount; ctr++)
    {
        sem_post(&m_sem);
    }
}

#endif