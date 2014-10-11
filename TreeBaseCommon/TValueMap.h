// TValueMap.h: interface for the TValueMap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TVALUEMAP_H__F9D9AEA2_3B77_4526_9F9B_5B89CF2CC90D__INCLUDED_)
#define AFX_TVALUEMAP_H__F9D9AEA2_3B77_4526_9F9B_5B89CF2CC90D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TreeBaseCommon.h"
#include "TreeBaseInt.h"

#define TBVALUE_ENTRY_COUNT 173
//#define TBVALUE_ENTRY_COUNT 1

struct TValueMapEntry
{
    TValueMapEntry *m_pNext;
    BOOL            m_bOwnBlock; 
    TBITEM_ENTRY   *m_pValue; 
    long            m_cbSize;
};

class __TBCOMMON_EXPORT__ TValueMap  
{
    void            DestroyItemEntry(TValueMapEntry *a_pEntry);
    TValueMapEntry *CreateItemEntry(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize);
    TValueMapEntry *GetFirstEntry(LPCSTR a_pszName);
    BOOL            SetValueCore(TValueMapEntry *a_pNewEntry);

private:
    TMemAlloc      *m_pMalloc;
    TValueMapEntry *m_entries[TBVALUE_ENTRY_COUNT];
    long            m_nCount;
public:
	void Shrink(long a_nNewSize);
	LPVOID ExportBatch();
	BOOL SetValueDeleted(LPCSTR a_pszValue);
	TBITEM_ENTRY* FindValue(LPCSTR a_pszName);
	void Clear();
	BOOL RemoveValue(LPCSTR a_pszName);
	BOOL SetValue(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize=-1);
    BOOL SetValue(TBITEM_ENTRY *a_pValue);
	TValueMap(TMemAlloc *a_pMalloc = NULL);
	virtual ~TValueMap();

    long GetCount()
    {
        return m_nCount;
    };
    inline void* MemAlloc(long a_cbSize)
    {
        return m_pMalloc ? m_pMalloc->Alloc(a_cbSize) : NULL;
    }
    inline void MemFree(LPVOID a_pvBuff)
    {
        m_pMalloc ? m_pMalloc->Free(a_pvBuff) : 0;
    }
    inline void* MemReAlloc(LPVOID a_pvBuff, long a_cbSize)
    {
        return m_pMalloc ? m_pMalloc->ReAlloc(a_pvBuff, a_cbSize) : NULL;
    }
};

class __TBCOMMON_EXPORT__ TValuePage  
{
public:
    BYTE m_data[PAGESIZE];
public:
	TValuePage();
	virtual ~TValuePage();

    TBITEM_ENTRY *GetNextItem(long &a_pnOffset);
    TBITEM_ENTRY *FindItem(LPCSTR a_pszName);
};


#endif // !defined(AFX_TVALUEMAP_H__F9D9AEA2_3B77_4526_9F9B_5B89CF2CC90D__INCLUDED_)
