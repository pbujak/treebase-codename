// TValueMap.cpp: implementation of the TValueMap class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TValueMap.h"
#include "TreeBaseInt.h"
#include "TreeBasePriv.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TValueMap::TValueMap(TMemAlloc *a_pMalloc): m_pMalloc(a_pMalloc ? a_pMalloc : G_pMalloc)
{
    m_nCount = 0;
    memset(m_entries, 0, TBVALUE_ENTRY_COUNT*sizeof(TValueMapEntry*));
}

TValueMap::~TValueMap()
{
    Clear();
}


TValuePage::TValuePage()
{

}

TValuePage::~TValuePage()
{

}

TBITEM_ENTRY *TValuePage::GetNextItem(long &a_pnOffset)
{
    TBITEM_ENTRY *pEntry = NULL;
    long          nType  = 0;

    pEntry = MAKE_PTR(TBITEM_ENTRY*, m_data, a_pnOffset);
    nType  = (long)pEntry->iType;
    if (nType==TBVITYPE_NOTUSED)
        return NULL;
    a_pnOffset += pEntry->wSize;
    return pEntry;
};

TBITEM_ENTRY *TValuePage::FindItem(LPCSTR a_pszName)
{
    TBITEM_ENTRY *pEntry = NULL;
    DWORD        dwHash  = HASH_ComputeHash(a_pszName);
    long         nOffset = 0;

    do
    {
        pEntry = GetNextItem(nOffset);
        if (pEntry && pEntry->dwHash==dwHash)
        {
            if (strcmpi(a_pszName, TBITEM_NAME(pEntry))==0)
                return pEntry;
        }
    }while (pEntry);
    return NULL;
}

//*****************************************************************************
void TValueMap::DestroyItemEntry(TValueMapEntry *a_pEntry)
{
    if (a_pEntry)
    {
        if (a_pEntry->m_pValue && a_pEntry->m_bOwnBlock)
            MemFree(a_pEntry->m_pValue);
        delete a_pEntry;
    }
}

//*****************************************************************************
TValueMapEntry* TValueMap::CreateItemEntry(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    TBITEM_ENTRY   *pValue = NULL;
    TValueMapEntry *pEntry = NULL;

    if (a_dwType==TBVITYPE_DELETED)
    {
        pValue = (TBITEM_ENTRY*)COMMON_CreateItemEntry(a_pszName, TBVTYPE_INTEGER, a_pValue, a_cbSize, m_pMalloc);
    }
    else
        pValue = (TBITEM_ENTRY*)COMMON_CreateItemEntry(a_pszName, a_dwType, a_pValue, a_cbSize, m_pMalloc);

    if (pValue==NULL)
        return NULL;

    pEntry = new TValueMapEntry;
    if (pEntry==NULL)
        return NULL;

    pEntry->m_pValue = pValue;
    pEntry->m_pNext  = NULL;
    pEntry->m_bOwnBlock = TRUE;
    pEntry->m_cbSize    = a_cbSize;

    if (a_dwType==TBVITYPE_DELETED)
        pEntry->m_pValue->iType = TBVITYPE_DELETED;

    return pEntry;
}

//*****************************************************************************
BOOL TValueMap::SetValue(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    DWORD           dwHash = 0;
    TValueMapEntry *pNewEntry  = NULL;
    //------------------------------------------------------------

    if (a_dwType == TBVTYPE_SECTION || a_dwType == TBVTYPE_LONGBINARY)
    {
        SYS_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName);
    }

    if (a_pszName==NULL)
        return FALSE;
    
    dwHash = HASH_ComputeHash(a_pszName);

    //------------------------------------------------------------
    pNewEntry = CreateItemEntry(a_pszName, a_dwType, a_pValue, a_cbSize);
    if (pNewEntry==NULL)
        return FALSE;

    pNewEntry->m_pValue->dwHash = dwHash;

    return SetValueCore(pNewEntry);
}

//*****************************************************************************
BOOL TValueMap::SetValue(TBITEM_ENTRY *a_pValue)
{
    DWORD           dwHash = 0;
    TValueMapEntry *pNewEntry  = NULL;
    TBITEM_ENTRY   *pNewValue  = NULL;
    long            cbSize     = 0;
    //------------------------------------------------------------
    pNewValue = (TBITEM_ENTRY *)MemAlloc(a_pValue->wSize);
    if (pNewValue==NULL)
        return FALSE;
    memcpy(pNewValue, a_pValue, a_pValue->wSize);

    cbSize = a_pValue->wSize - (long)a_pValue->iValue;
    //------------------------------------------------------------
    pNewEntry = new TValueMapEntry;
    if (pNewEntry==NULL)
    {
        free(pNewValue);
        return NULL;
    }

    //------------------------------------------------------------
    pNewEntry->m_pValue = pNewValue;
    pNewEntry->m_pNext  = NULL;
    pNewEntry->m_bOwnBlock = TRUE;
    pNewEntry->m_cbSize    = cbSize;

    return SetValueCore(pNewEntry);
}


//*****************************************************************************
BOOL TValueMap::SetValueCore(TValueMapEntry *a_pNewEntry)
{
    TBITEM_ENTRY   *pValue = NULL;
    DWORD           dwHash = 0;
    long            nIdx   = 0; 
    TValueMapEntry *pEntryPrev = NULL;
    TValueMapEntry *pEntryNext = NULL;
    TValueMapEntry *pEntry     = NULL;
    LPCSTR          pszName    = NULL; 
    LPCSTR          pszNewName = NULL; 


    dwHash = a_pNewEntry->m_pValue->dwHash;
    nIdx   = dwHash % TBVALUE_ENTRY_COUNT;
    pszNewName = TBITEM_NAME(a_pNewEntry->m_pValue);

    pEntry = m_entries[nIdx];
    //------------------------------------------------------------
    if (pEntry==NULL)
    {
        m_entries[nIdx] = a_pNewEntry;
        m_nCount++;
    }
    //------------------------------------------------------------
    else
    {
        while (pEntry!=NULL)
        {
            pszName = TBITEM_NAME(pEntry->m_pValue);
            if (stricmp(pszName, pszNewName)==0)
            {
                pEntryNext = pEntry->m_pNext;
                DestroyItemEntry(pEntry);
                a_pNewEntry->m_pNext = pEntryNext;
                if (pEntryPrev==NULL)
                {
                   m_entries[nIdx] = a_pNewEntry;
                }
                else
                {
                   pEntryPrev->m_pNext = a_pNewEntry; 
                }
                return TRUE;
            }
            pEntryPrev = pEntry;
            pEntry = pEntry->m_pNext;
        }
        pEntryPrev->m_pNext = a_pNewEntry;
        m_nCount++;
    }
    //------------------------------------------------------------
    return TRUE;
}

//*****************************************************************************
BOOL TValueMap::RemoveValue(LPCSTR a_pszName)
{
    LPCSTR          pszName = NULL;
    TBITEM_ENTRY   *pValue = NULL;
    DWORD           dwHash = 0;
    long            nIdx   = 0; 
    TValueMapEntry *pEntryPrev = NULL;
    TValueMapEntry *pEntry     = NULL;

    //------------------------------------------------------------
    if (a_pszName==NULL)
        return FALSE;
    
    dwHash = HASH_ComputeHash(a_pszName);
    nIdx   = dwHash % TBVALUE_ENTRY_COUNT;

    pEntry = m_entries[nIdx];

    //------------------------------------------------------------
    while (pEntry!=NULL)
    {
        pszName = TBITEM_NAME(pEntry->m_pValue);
        if (stricmp(a_pszName, pszName)==0)
        {
            if (pEntryPrev)
            {
                pEntryPrev->m_pNext = pEntry->m_pNext;
            }
            else
                m_entries[nIdx] = pEntry->m_pNext;
            DestroyItemEntry(pEntry);
            m_nCount--;
            return TRUE;
        }
        pEntryPrev = pEntry;
        pEntry = pEntry->m_pNext;
    }
    return FALSE;
}

//*****************************************************************************
void TValueMap::Clear()
{
    long ctr = 0;
    TValueMapEntry *pEntry = NULL;
    TValueMapEntry *pNext = NULL;

    for (ctr=0; ctr<TBVALUE_ENTRY_COUNT; ctr++)
    {
        pEntry = m_entries[ctr];
        while (pEntry!=NULL)
        {
            pNext = pEntry->m_pNext;
            if (pEntry->m_bOwnBlock)
                MemFree(pEntry->m_pValue);
            delete pEntry;
            pEntry = pNext;
        }
    }
    memset(&m_entries, 0, sizeof(m_entries));
    m_nCount = 0;
}

//*****************************************************************************
TBITEM_ENTRY* TValueMap::FindValue(LPCSTR a_pszName)
{
    LPCSTR          pszName = NULL;
    TBITEM_ENTRY   *pValue  = NULL;
    TValueMapEntry *pEntry  = NULL;

    pEntry = GetFirstEntry(a_pszName);

    //------------------------------------------------------------
    while (pEntry!=NULL)
    {
        pszName = TBITEM_NAME(pEntry->m_pValue);
        if (stricmp(pszName, a_pszName)==0)
            return pEntry->m_pValue;
        pEntry = pEntry->m_pNext;
    }
    //------------------------------------------------------------
    return NULL;
}

//*****************************************************************************
TValueMapEntry *TValueMap::GetFirstEntry(LPCSTR a_pszName)
{
    DWORD           dwHash  = 0;
    long            nIdx    = 0; 

    //------------------------------------------------------------
    if (a_pszName==NULL)
        return NULL;
    
    dwHash = HASH_ComputeHash(a_pszName);
    nIdx   = dwHash % TBVALUE_ENTRY_COUNT;
    //------------------------------------------------------------
    return m_entries[nIdx];
}

//*****************************************************************************
BOOL TValueMap::SetValueDeleted(LPCSTR a_pszName)
{
    TBVALUE_ENTRY  val = {0};

    return SetValue(a_pszName, TBVITYPE_DELETED, &val);
}

//*****************************************************************************
LPVOID TValueMap::ExportBatch()
{
    TBITEM_ENTRY   *pItem       = NULL; 
    TB_BATCH_ENTRY *pBatchEntry = NULL;
    TValueMapEntry *pMapEntry = NULL;
    long            nBatchSize = 512;
    long            nOffset = 0;
    long            nPrevOffset = -1;
    LPVOID          pvBatch = NULL, pvBatchNew = NULL;
    long            ctr    = 0; 
    long            cbSize = 0; 

    //------------------------------------------------------------
    pvBatch = SYS_MemAlloc(nBatchSize);
    if (pvBatch==NULL)
        return NULL;
    //------------------------------------------------------------
    for (ctr=0; ctr<TBVALUE_ENTRY_COUNT; ctr++)
    {
        pMapEntry = m_entries[ctr];
        while (pMapEntry!=NULL)
        {
            pItem = pMapEntry->m_pValue;
            cbSize = pItem->wSize + offsetof(TB_BATCH_ENTRY,item);

            if (nOffset+cbSize>nBatchSize)
            {
                nBatchSize += 512;
                pvBatchNew = SYS_MemReAlloc(pvBatch, nBatchSize);
                if (pvBatchNew==NULL)
                {
                    MemFree(pvBatch);
                    return FALSE;
                }
                pvBatch = pvBatchNew;
            }
            pBatchEntry = MAKE_PTR(TB_BATCH_ENTRY*, pvBatch, nOffset);
            pBatchEntry->ofsNext = 0;
            pBatchEntry->action  = (pItem->iType==TBVITYPE_DELETED ? eActionDel : eActionSet);
            pBatchEntry->cbSize  = pMapEntry->m_cbSize;
            memcpy(&pBatchEntry->item, pItem, pItem->wSize);

            if (nPrevOffset != -1)
            {
                pBatchEntry = MAKE_PTR(TB_BATCH_ENTRY*, pvBatch, nPrevOffset);
                pBatchEntry->ofsNext = nOffset;
            }
            nPrevOffset = nOffset;
            nOffset += cbSize;
    
            pMapEntry = pMapEntry->m_pNext;
        }
    }
    //------------------------------------------------------------
    return pvBatch;
}

//*****************************************************************************
void TValueMap::Shrink(long a_nNewSize)
{
    TValueMapEntry *pMapEntry = NULL;
    TValueMapEntry *pNext     = NULL;
    long            ctr=0;

    if (a_nNewSize<=0)
    {
        Clear();
        return;
    }

    if (a_nNewSize>=m_nCount)
        return;
    //------------------------------------------------------------
    do
    {
        pMapEntry = m_entries[ctr];
        if (pMapEntry!=NULL)
        {
            pNext = pMapEntry->m_pNext;
            DestroyItemEntry(pMapEntry);
            m_entries[ctr] = pNext;
            m_nCount--;
        }
        ctr = (++ctr) % TBVALUE_ENTRY_COUNT;
    }
    while(m_nCount > a_nNewSize);
}
