// TTask.cpp: implementation of the TTask class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TTask.h"
#include "SegmentMgr.h"
#include "DataMgr.h"
#include <malloc.h>
#include "Section.h"
#include "blob.h"
#include "label.h"
#include "Storage.h"
#include "Shared\TAbstractThreadLocal.h"

IMPLEMENT_INIT(TTask::GlobalInit)

typedef struct
{
    DWORD m_dwPool;
    char  m_data[1];
}MEMSEGMENT_HEADER;

#define POOL_GLOBAL 0
#define POOL_TASK   1

template<>
inline void DeletePointer(TSection *a_pSection)
{
    a_pSection->Release();
}

//*************************************************************************
DWORD WINAPI TaskEntry(LPVOID lpParameter)
{
    TTask::TaskParams *pParams = (TTask::TaskParams *)lpParameter;

    if(pParams->pTask->RunInCurrentThread(pParams->nHeapSize, pParams->nMaxHeapSize))
    {
        _release_ptr(pParams->pTask);
    };
    delete pParams;
    return 1;
};

//==============================================================================
LONG               TTask::S_dwCount        = 0;

static TThreadLocal<TTask*> s_pTask;

//*************************************************************************
TTask *TASK_GetCurrent()
{
    return s_pTask;
};

//*************************************************************************
LPVOID TASK_MemDup(LPVOID a_pvData)
{
    if(a_pvData == NULL)
        return NULL;

    int cbSize = TASK_MemSize(a_pvData);
    LPVOID pvData2 = TASK_MemAlloc(cbSize);
    memcpy(pvData2, a_pvData, cbSize);
    return pvData2;
}

//*************************************************************************
LPVOID TASK_MemAlloc(DWORD a_cbSize)
{
    TTask *pTask = TASK_GetCurrent();
    if (pTask)
    {
        return HeapAlloc(pTask->m_hHeap, HEAP_NO_SERIALIZE, a_cbSize);
    }
    else
        return malloc((size_t)a_cbSize);
}

//*************************************************************************
void TASK_MemFree(LPVOID a_pvData)
{
    TTask *pTask = TASK_GetCurrent();
    if (pTask)
    {
        HeapFree(pTask->m_hHeap, HEAP_NO_SERIALIZE, a_pvData);
    }
    else
        free(a_pvData);
}

//*************************************************************************
LPVOID TASK_MemReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    TTask *pTask = TASK_GetCurrent();
    if (pTask)
    {
        return HeapReAlloc(pTask->m_hHeap, HEAP_NO_SERIALIZE, a_pvData, a_cbSize);
    }
    else
        return realloc(a_pvData, (size_t)a_cbSize);
}

//*************************************************************************
DWORD TASK_MemSize(LPVOID a_pvData)
{
    TTask *pTask = TASK_GetCurrent();
    if (pTask)
    {
        return HeapSize(pTask->m_hHeap, HEAP_NO_SERIALIZE, a_pvData);
    }
    else
        return _msize(a_pvData);
}


//*************************************************************************
void TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)
{
    TTask *pTask = TASK_GetCurrent();

    if (pTask!=NULL)
    {
        if (a_dwError>0)
            pTask->m_dwError = a_dwError;

        if (a_pszErrorItem)
            pTask->m_strErrorItem = a_pszErrorItem;
        if (a_pszSection)
            pTask->m_strErrorSection = a_pszSection;
    }
};

//*************************************************************************
void TASK_ClearErrorInfo()
{
    TTask *pTask = TASK_GetCurrent();

    if (pTask!=NULL)
    {
        pTask->m_dwError = 0;
        pTask->m_strErrorItem.Empty();
        pTask->m_strErrorSection.Empty();
    }
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TTask::TTask()
{
    m_pRoot = NULL;
    m_hEvent = NULL;
    m_pvBlobBuf = NULL;
    m_pLabel = NULL;
    m_pLabel2 = NULL;
}

TTask::~TTask()
{
    CloseHandle(m_hEvent);
}

BOOL TTask::GlobalInit()
{
    return TRUE;
}

void TTask::Join()
{
    THandle hThread = OpenThread(SYNCHRONIZE, FALSE, m_dwThreadId);
    WaitForSingleObject(hThread, INFINITE);
}

BOOL TTask::RunInCurrentThread(int a_nHeapSize, int a_nMaxHeapSize)
{
    s_pTask = const_cast<TTask*>(this);
    InterlockedIncrement(&TTask::S_dwCount);
    
    m_hHeap = HeapCreate(HEAP_NO_SERIALIZE, a_nHeapSize, a_nMaxHeapSize);
    if(m_hHeap == NULL)
        return FALSE;

    BOOL bOK = OnStart();
    m_bCreateOK = bOK;
    if(m_hEvent)
        SetEvent(m_hEvent);

    if(!bOK)
        return FALSE;

    if(m_hEvent == NULL)
        m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    OnBeforeRun();
    OnRun();
    OnFinish();

    InterlockedDecrement(&TTask::S_dwCount);
    HeapDestroy(m_hHeap);
    m_hHeap = NULL;
    TAbstractThreadLocal::ClearAllData();
    return 1;
}

BOOL TTask::Start(long a_nPriority, int a_nHeapSize, int a_nMaxHeapSize)
{
    HANDLE hThread = NULL;

    if (m_hEvent != NULL)
    {
        ResetEvent(m_hEvent);
    }
    else
        m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_hEvent)
    {
        TaskParams *pParams = new TaskParams;
        pParams->pTask = this;
        pParams->nHeapSize = a_nHeapSize;
        pParams->nMaxHeapSize = a_nMaxHeapSize;

        hThread = CreateThread(NULL, 0, &TaskEntry, pParams, 0, &m_dwThreadId);
        if (hThread)
        {
            if (a_nPriority!=THREAD_PRIORITY_NORMAL)
                SetThreadPriority(hThread, a_nPriority);
            CloseHandle(hThread);
            WaitForSingleObject(m_hEvent, INFINITE);
            return m_bCreateOK;
        }
    }
    return FALSE;
}


//*************************************************************
BOOL TTask::OnStart()
{
    return TRUE;
}

//*************************************************************
void TTask::OnBeforeRun()
{
}

//*************************************************************
TSection* TTask::GetRootSection()
{
    if(m_pRoot == NULL)
        m_pRoot = new TRootSection();
    return m_pRoot;
}

//*************************************************************
TLabelSection* TTask::GetPrimaryLabelSection()
{
    if(m_pLabel == NULL)
        m_pLabel = new TLabelSection();
    return m_pLabel;
}


//*************************************************************
void TTask::OnFinish()
{
    _release_ptr(m_pRoot)
    _release_ptr(m_pLabel)
    _release_ptr(m_pLabel2)

    Util::TTaskObjectSet<Stream::TInputStream> isset;
    isset.removeAll();

    Util::TTaskObjectSet<Stream::TOutputStream> osset;
    osset.removeAll();

    // Must be after closing streams
    Util::TTaskObjectSet<TSection> sset;
    sset.removeAll();

    FILE_Flush();
}

//************************************************************************
LPVOID TTaskMemAlloc::Alloc(DWORD a_cbSize)
{
    return TASK_MemAlloc(a_cbSize);
};

//************************************************************************
void TTaskMemAlloc::Free(LPVOID a_pvData)
{
    TASK_MemFree(a_pvData);
};

//************************************************************************
LPVOID TTaskMemAlloc::ReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return TASK_MemReAlloc(a_pvData, a_cbSize);
};

//************************************************************************
DWORD  TTaskMemAlloc::MemSize(LPVOID a_pvData)
{
    return TASK_MemSize(a_pvData);
};

//************************************************************************
LPVOID TAppMemAlloc::Alloc(DWORD a_cbSize)
{
    return malloc(a_cbSize);
};

//************************************************************************
void TAppMemAlloc::Free(LPVOID a_pvData)
{
    free(a_pvData);
};

//************************************************************************
LPVOID TAppMemAlloc::ReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return realloc(a_pvData, a_cbSize);
};

//************************************************************************
DWORD  TAppMemAlloc::MemSize(LPVOID a_pvData)
{
    return _msize(a_pvData);
};

//************************************************************************
void TSpinLock::Lock()
{
    if (m_nCount==0)
        EnterCriticalSection(&m_cs);
    InterlockedIncrement(&m_nCount);
};

//************************************************************************
void TSpinLock::Unlock()
{
    InterlockedDecrement(&m_nCount);
    if (m_nCount==0)
        LeaveCriticalSection(&m_cs);
};

//************************************************************************
TUseSpinLock::TUseSpinLock (TSpinLock *a_pSpinLock)
{
    if (a_pSpinLock)
    {
        a_pSpinLock->Lock();
        m_pSpinLock = a_pSpinLock;
    }
};

//************************************************************************
TUseSpinLock::~TUseSpinLock()
{
    if (m_pSpinLock)
        m_pSpinLock->Unlock();
};

//************************************************************************
TLabelSection* TTask::GetLabelSection(long a_ndbId)
{
    if(a_ndbId==0)
        return GetPrimaryLabelSection();

    if(m_pLabel2)
    {
        if(m_pLabel2->m_ndbId != a_ndbId)
        {
            delete m_pLabel2;
            m_pLabel2 = NULL;
        }
    }
    if(m_pLabel2==NULL)
    {
        m_pLabel2 = new TLabelSection(a_ndbId);
    }
    return m_pLabel2;
}

//************************************************************************
TLabelSection* TASK_GetLabelSection(long a_ndbId)
{
    TTask *pTask = TASK_GetCurrent();

    if(pTask)
        return pTask->GetLabelSection(a_ndbId);
    return NULL;
}

