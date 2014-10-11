#pragma once
#include "TreeBase.h"
#include "TreeBaseInt.h"
#include <stdlib.h>

inline LPVOID TASK_MemAlloc(DWORD a_cbSize)
{
    return malloc(a_cbSize);
}

inline void TASK_MemFree(LPVOID a_pvData)
{
    free(a_pvData);
}

inline LPVOID TASK_MemReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return realloc(a_pvData, a_cbSize);
}

inline DWORD TASK_MemSize(LPVOID a_pvData)
{
    return _msize(a_pvData);
}

LPVOID TASK_MemDup(LPVOID a_pvData);

class TTask
{
public:
    TTask(void);
public:
    ~TTask(void);

    DWORD m_dwError;
    CString m_strErrorItem;
    CString m_strErrorSection;

    static DWORD S_dwCount;
};

class TTaskMemAlloc: public TMemAlloc
{
public:
    virtual LPVOID Alloc(DWORD a_cbSize);
    virtual void   Free(LPVOID a_pvData);
    virtual LPVOID ReAlloc(LPVOID a_pvData, DWORD a_cbSize);
    virtual DWORD  MemSize(LPVOID a_pvData);
};

template<class T>
class TTaskAutoPtr
{
    T* m_ptr;
public:
    inline TTaskAutoPtr(T* a_ptr = NULL): m_ptr(a_ptr) {}

    inline operator T*()
    {
        return m_ptr;
    }
    inline T* operator->()
    {
        return m_ptr;
    }
    inline TTaskAutoPtr& operator=(T *a_ptr)
    {
        if(m_ptr)
            TASK_MemFree(m_ptr);
        m_ptr = a_ptr;
        return *this;
    }
    inline bool realloc(int a_nSize)
    {
        if(m_ptr)
        {
            T *ptr = (T*)TASK_MemReAlloc(m_ptr, a_nSize);
            if(ptr == NULL)
                return false;
            m_ptr = ptr;
            return TRUE;
        }
        return FALSE;
    }

    inline T* ptr()
    {
        return m_ptr;
    }

    inline T* detach()
    {
        T *p = m_ptr;
        m_ptr = NULL;
        return p;
    }

    ~TTaskAutoPtr()
    {
        if(m_ptr)
            TASK_MemFree(m_ptr);
    }
};


TTask* TASK_GetCurrent();
void   TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem=NULL, LPCSTR a_pszErrorSection=NULL);

typedef BOOL (*INIT_PROC)();

class TInitialize
{
public:
    TInitialize(INIT_PROC a_pProc)
    {
       (*a_pProc)();
    }
};
