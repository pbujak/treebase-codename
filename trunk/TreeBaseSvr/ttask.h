// TTask.h: interface for the TTask class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(TASK_H)
#define TASK_H

#include "Util.h"
#include "stream.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#pragma warning(disable: 4786)

#include "TreeBaseInt.h"
#include <set>
#include "syncobj.h"

#define WM_TASKMESSAGE   WM_APP + 0x0

#define CODE_ENSURE_FILE_OFFSET 0

namespace Storage
{
    class TStorage;
};
class TTask;
class TSection;
class TRootSection;
class TLabelSection;
class TLongBinary;

typedef std::set<TSection*>           TSectionSet;
typedef std::set<TSection*>::iterator TSectionSetIter;
typedef std::set<Stream::TInputStream*>            TInputStreamSet;
typedef std::set<Stream::TInputStream*>::iterator  TInputStreamSetIter;
typedef std::set<Stream::TOutputStream*>           TOutputStreamSet;
typedef std::set<Stream::TOutputStream*>::iterator TOutputStreamSetIter;

typedef struct{
    long    Code;
    TTask  *pTask;
    WPARAM  wParam;
    LPARAM  lParam;
    BOOL    bReturnValue;
}TASK_MESSAGE;

typedef BOOL (*TASK_MESSAGE_PROC)(long a_nCode, WPARAM a_wParam, LPARAM a_lParam);


TTask  *TASK_GetCurrent();
LPVOID  TASK_MemDup(LPVOID a_pvData);
LPVOID  TASK_MemAlloc(DWORD a_cbSize);
void    TASK_MemFree(LPVOID a_pvData);
LPVOID  TASK_MemReAlloc(LPVOID a_pvData, DWORD a_cbSize);
DWORD   TASK_MemSize(LPVOID a_pvData);
void    TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem=NULL, LPCSTR a_pszErrorSection=NULL);
void    TASK_ClearErrorInfo();
CString TASK_GetComputerName();
TLabelSection* TASK_GetLabelSection(long a_ndbId);

class TTask  
{
    friend TTask        *TASK_GetCurrent();
    friend DWORD WINAPI  TaskEntry(LPVOID lpParameter);

    struct TaskParams
    {
        TTask *pTask;
        int    nHeapSize;
        int    nMaxHeapSize;
    };

    HANDLE m_hEvent;
    BOOL   m_bCreateOK;
public:
    HANDLE m_hHeap;
    DWORD  m_dwThreadId;

    static LONG S_dwCount;
public:
	static BOOL GlobalInit();
	TTask();
	virtual ~TTask();
    TSection* GetRootSection();
    TLabelSection* GetPrimaryLabelSection();

public:
    virtual void OnBeforeRun();
    virtual BOOL OnStart();
    virtual void OnRun() = 0;
    virtual void OnFinish(); 
	TLabelSection* GetLabelSection(long a_ndbId);
public:
    void Join();
    BOOL RunInCurrentThread(int a_heapSize = 0x200000, int a_maxHeapSize = 0);
    BOOL Start(long a_nPriority = THREAD_PRIORITY_NORMAL, int a_heapSize = 0x200000, int a_maxHeapSize = 0);
    BOOL Wait(DWORD a_dwMilis = INFINITE)
    {
        return WaitForSingleObject(m_hEvent, a_dwMilis)==WAIT_OBJECT_0;
    }
    void Notify()
    {
        SetEvent(m_hEvent);
    }

    TRootSection    *m_pRoot;
    TLabelSection   *m_pLabel, *m_pLabel2;

    DWORD    m_dwError; 
    CString  m_strErrorItem;
    CString  m_strErrorSection;
    LPVOID   m_pvBlobBuf;
};

class TTaskMemAlloc: public TMemAlloc
{
public:
    virtual LPVOID Alloc(DWORD a_cbSize);
    virtual void   Free(LPVOID a_pvData);
    virtual LPVOID ReAlloc(LPVOID a_pvData, DWORD a_cbSize);
    virtual DWORD  MemSize(LPVOID a_pvData);
};

class TAppMemAlloc: public TMemAlloc
{
public:
    virtual LPVOID Alloc(DWORD a_cbSize);
    virtual void   Free(LPVOID a_pvData);
    virtual LPVOID ReAlloc(LPVOID a_pvData, DWORD a_cbSize);
    virtual DWORD  MemSize(LPVOID a_pvData);
};

class TSpinLock
{
    CRITICAL_SECTION m_cs;
    long             m_nCount;
public:
    TSpinLock()
    {
        m_nCount = 0;
        InitializeCriticalSection(&m_cs);
    }
    void Lock();
    void Unlock();
};

class TUseSpinLock
{
    TSpinLock *m_pSpinLock;
public:
    TUseSpinLock (TSpinLock *a_pSpinLock = NULL);
    ~TUseSpinLock();
};

#endif // !defined(TASK_H)
