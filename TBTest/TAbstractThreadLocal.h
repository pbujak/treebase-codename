// TAbstractThreadLocal.h: interface for the TAbstractThreadLocal class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(_THREAD_LOCAL_BASE_H_)
#define _THREAD_LOCAL_BASE_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <afxtempl.h>

class TAbstractThreadLocal  
{
    static TAbstractThreadLocal *S_pFirst;
    static long              S_nSizeTotal;
    static long              S_tlsIndex;

protected:
    TAbstractThreadLocal       *m_pNext;
    long                    m_nOffset;
private:
    TAbstractThreadLocal(){};
public:
	void* GetData();
	TAbstractThreadLocal(long a_nSize);
	virtual ~TAbstractThreadLocal();

    static void ClearAllData();
protected:
    virtual void InitData(void *a_pvData)=0;
    virtual void ClearData(void *a_pvData)=0;
};

template<class T>
class TThreadLocal: public TAbstractThreadLocal
{
    T* GetItem()
    {
        void *pvData = GetData();
        T* pData = (T*)((BYTE*)pvData + m_nOffset);
        return pData;
    }
public:
    TThreadLocal(): TAbstractThreadLocal(sizeof(T))
    {
    }
    operator T&()
    {
        T* pData = GetItem();
        return *pData;
    }
    T& Value()
    {
        T* pData = GetItem();
        return *pData;
    }
    T* operator&()
    {
        T* pData = GetItem();
        return pData;
    }
    T& operator=(T &a_src)
    {
        T* pData = GetItem();
        *pData = a_src;
        return *this;
    }
protected:
    virtual void InitData(void *a_pvData)
    {
        T* pData = (T*)((BYTE*)a_pvData + m_nOffset);
        new(pData) T();
    };
    virtual void ClearData(void *a_pvData)
    {
        T* pData = (T*)((BYTE*)a_pvData + m_nOffset);
        pData->~T();
    };
};

#endif // !defined(_THREAD_LOCAL_BASE_H_)
