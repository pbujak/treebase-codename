/* 
 * File:   rwlock.h
 * Author: pabu
 *
 * Created on 25 January 2013, 09:24
 */

#ifndef SYNCOBJ_H
#define	SYNCOBJ_H


#ifdef _WIN32
  // Windows specific

typedef struct _RWLOCK {
    CRITICAL_SECTION internalLock;
    CRITICAL_SECTION writerLock;
    HANDLE noReaders;
    int    readerCount;
    bool   waitNoReaders;
} RWLOCK, *PRWLOCK;

void RWLock_Initialize(PRWLOCK prwLock);
void RWLock_ReadLock(PRWLOCK prwLock);
void RWLock_WriteLock(PRWLOCK prwLock);
void RWLock_ReadUnlock(PRWLOCK prwLock);
void RWLock_WriteUnlock(PRWLOCK prwLock);
void RWLock_Delete(PRWLOCK prwLock);

class TSemaphore
{
    HANDLE m_hSem;
public:    
    inline TSemaphore(int a_nCount = 0)
    {
        m_hSem = CreateSemaphore(NULL, a_nCount, 100, NULL);
    }
    inline ~TSemaphore()
    {
        CloseHandle(m_hSem);
    }
    
    inline void down()
    {
        WaitForSingleObject(m_hSem, INFINITE);
    }
    inline bool down(int a_nMillis)
    {
        return WaitForSingleObject(m_hSem, a_nMillis) == WAIT_OBJECT_0;
    }
    inline void up(int a_nCount = 1)
    {
        LONG lPrev;
        ReleaseSemaphore(m_hSem, a_nCount, &lPrev);
    }
};


#else // WIN32

#include <pthread.h>
#include <semaphore.h>

typedef pthread_mutex_t   CRITICAL_SECTION;
typedef pthread_rwlock_t  RWLOCK;

inline void InitializeCriticalSection(CRITICAL_SECTION *pcs)
{ 
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(pcs, &attr);
}

#define EnterCriticalSection pthread_mutex_lock
#define LeaveCriticalSection pthread_mutex_unlock
#define DeleteCriticalSection pthread_mutex_destroy

// RWLock
#define RWLock_Initialize(x)  pthread_rwlock_init(x, NULL)
#define RWLock_ReadLock       pthread_rwlock_rdlock
#define RWLock_WriteLock      pthread_rwlock_wrlock

#define RWLock_ReadUnlock     pthread_rwlock_unlock
#define RWLock_WriteUnlock    pthread_rwlock_unlock
#define RWLock_Delete         pthread_rwlock_destroy

// Semaphore

class TSemaphore
{
    sem_t m_sem;
public:    
    inline TSemaphore(int a_nCount = 0)
    {
        sem_init(&m_sem, 0, a_nCount);
    }
    inline ~TSemaphore()
    {
        sem_destroy(&m_sem);
    }
    
    inline void down()
    {
        sem_wait(&m_sem);
    }
    inline void down(int a_nMillis)
    {
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += a_nMillis * 1000;
        sem_timedwait(m_hSem, a_nMillis);
    }
    void up(int a_nCount = 1);
};

#endif // WIN32

#define RWLock_LockShared      RWLock_ReadLock
#define RWLock_LockExclusive   RWLock_WriteLock
#define RWLock_UnlockShared    RWLock_ReadUnlock    
#define RWLock_UnlockExclusive RWLock_WriteUnlock   

class TCriticalSection
{
    CRITICAL_SECTION m_cs;
    
public:   
    inline TCriticalSection()
    {
        InitializeCriticalSection(&m_cs);
    }
    inline ~TCriticalSection()
    {
        DeleteCriticalSection(&m_cs);
    }
    inline CRITICAL_SECTION* operator&()
    {
        return &m_cs;
    }
};


class TReadWriteLock
{
    RWLOCK m_rwLock;
    
public:   
    inline TReadWriteLock()
    {
        RWLock_Initialize(&m_rwLock);
    }
    inline ~TReadWriteLock()
    {
        RWLock_Delete(&m_rwLock);
    }
    inline RWLOCK* operator&()
    {
        return &m_rwLock;
    }
};

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

class TReadLock
{
    RWLOCK *m_prwLock;
public:
    inline TReadLock(RWLOCK *a_prwLock): m_prwLock(a_prwLock)
    {
        RWLock_ReadLock(a_prwLock);
    }
    inline ~TReadLock()
    {
        RWLock_ReadUnlock(m_prwLock);
    }
    inline void unlock()
    {
        RWLock_ReadUnlock(m_prwLock);
    }
    inline void lock()
    {
        RWLock_ReadLock(m_prwLock);
    }
};

class TWriteLock
{
    RWLOCK *m_prwLock;
public:
    inline TWriteLock(RWLOCK *a_prwLock): m_prwLock(a_prwLock)
    {
        RWLock_WriteLock(a_prwLock);
    }
    inline ~TWriteLock()
    {
        RWLock_WriteUnlock(m_prwLock);
    }
    inline void unlock()
    {
        RWLock_WriteUnlock(m_prwLock);
    }
    inline void lock()
    {
        RWLock_WriteLock(m_prwLock);
    }
};


#endif	/* SYNCOBJ_H */

