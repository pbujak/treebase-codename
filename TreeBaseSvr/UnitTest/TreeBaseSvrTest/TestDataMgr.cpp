#include "stdafx.h"
#if defined(UNIT_TEST)
#define MOCK_SEGMENT
#endif
#include "defs.h"
#include "..\..\TStoragePageAlloc.h"
#include "..\..\DataMgr.h"
#include "MockDBManager.h"
#include "MockStorage.h"
#include "TValueMap.h"
#include <vector>
#include <set>

static TTaskAutoPtr<void> s_pvFetchSeg;

class CTBValueEntry
{
    char m_szBuff[1000];
    int  m_nType;
public:
    CTBValueEntry(int a_nVal)
    {
        TBVALUE_ENTRY* pVal = (TBVALUE_ENTRY*)m_szBuff;
        pVal->int32 = a_nVal;
        m_nType = TBVTYPE_INTEGER;
    }
    CTBValueEntry(const CString &a_strVal)
    {
        TBVALUE_ENTRY* pVal = (TBVALUE_ENTRY*)m_szBuff;
        strcpy(pVal->text, a_strVal);
        m_nType = TBVTYPE_TEXT;
    }
    operator TBVALUE_ENTRY*()
    {
        return(TBVALUE_ENTRY*)m_szBuff;
    }
    inline int getType()
    {
        return m_nType;
    }
};

//=======================================================================
class SectionIterator
{
    TSectionSegmentIterator m_iter;
    TSectionSegment        *m_psb;
    ITERATED_ITEM_ENTRY   *m_pBlock, *m_pCurrent;

public:
    SectionIterator(TSectionSegment &a_sb, TBASE_ITEM_FILTER *a_pFilter = NULL)
        : m_iter(a_pFilter), m_pBlock(NULL), m_pCurrent(NULL), m_psb(&a_sb)
    {
    }
    ITERATED_ITEM_ENTRY* getFirst();
    ITERATED_ITEM_ENTRY* getNext();

    ~SectionIterator()
    {
        if(m_pBlock)
            TASK_MemFree(m_pBlock);
    }
};

//***********************************************************************
ITERATED_ITEM_ENTRY* SectionIterator::getFirst()
{
    if(m_pBlock)
        TASK_MemFree(m_pBlock);

    m_pBlock = m_iter.getNextItems(m_psb);
    m_pCurrent = m_pBlock;

    return getNext();
}

//***********************************************************************
ITERATED_ITEM_ENTRY *SectionIterator::getNext()
{
    if(m_pCurrent->wSize == ITERATOR_END_ALL)
    {
        return NULL;
    }
    else if(m_pCurrent->wSize == ITERATOR_END_BLOCK)
    {
        TASK_MemFree(m_pBlock);
        m_pBlock = m_iter.getNextItems(m_psb);
        m_pCurrent = m_pBlock;
    }

    ITERATED_ITEM_ENTRY *pResult = m_pCurrent;
    m_pCurrent = MAKE_PTR(ITERATED_ITEM_ENTRY*, m_pCurrent, m_pCurrent->wSize);
    return pResult;
}


//***********************************************************************
template<typename T>
void SetValue(TValueMap &a_xMap, const char* a_pszName, int a_nIndex, const T& a_value)
{
    CTBValueEntry val(a_value);

    CString strName;
    strName.Format("%s %d", a_pszName, a_nIndex);
    a_xMap.SetValue(strName, val.getType(), val);
}

//***********************************************************************
template<typename T>
void SetValue(TSectionSegment &a_sb, const char* a_pszName, int a_nIndex, const T& a_value)
{
    CTBValueEntry val(a_value);

    CString strName;
    strName.Format("%s %d", a_pszName, a_nIndex);

    BPOINTER bpElem = a_sb.FindValue(a_pszName);
    if (bpElem != 0)
    {
        a_sb.ModifyValue(a_pszName, NULL, val.getType(), val);
    }
    else
    {
        a_sb.AddValue(a_pszName, val.getType(), val);
    }
}

//***********************************************************************
bool ValidateSectionSegment(TSectionSegment &a_sb)
{
    Storage::TAdvPageRef xRef;
    int                    effPageSize = COMMON_PageSizeAdjustedToFibonacci();

    int nCount = a_sb.GetPageCount();

    for(int i = 0; i < nCount; i++)
    {
        LPVOID pvData = a_sb.GetPageForRead(i, xRef);
        int    nOffset = 0;

        while(nOffset < effPageSize)
        {
            TBITEM_ENTRY *pItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
            if(pItem->iType == TBVITYPE_NOTUSED)
                break;

            if (pItem->iType != TBVITYPE_DELETED && 
                pItem->iType != TBVITYPE_RESERVED)
            {
                LPCSTR pszName = TBITEM_NAME(pItem);
                BPOINTER bptr = a_sb.FindValue(pszName);
                
                if(bptr == 0)
                {
                    ASSERT(false);
                    return false;
                }
            }
            ASSERT(pItem->wSize > 0);
            nOffset += (long)pItem->wSize;
        }
    }
    return true;
}

//***********************************************************************
int GetSectionCount(TSectionSegment &a_sb, TBASE_ITEM_FILTER *a_pFilter = NULL)
{
    SectionIterator it(a_sb, a_pFilter);

    int nCount = 0;

    ITERATED_ITEM_ENTRY *pEntry = it.getFirst();

    while(pEntry)
    {
        nCount++;
        pEntry = it.getNext();
    }
    return nCount;
}

//***********************************************************************
void GetSectionNames(std::vector<std::string> &a_names, TSectionSegment &a_sb, TBASE_ITEM_FILTER *a_pFilter = NULL)
{
    SectionIterator it(a_sb, a_pFilter);

    int nCount = 0;

    ITERATED_ITEM_ENTRY *pEntry = it.getFirst();

    while(pEntry)
    {
        a_names.push_back(pEntry->szName);
        pEntry = it.getNext();
    }
}

//***********************************************************************
bool SetValueDeleted(TValueMap &a_xMap, const char* a_pszName, int a_nIndex)
{
    CString strName;
    strName.Format("%s %d", a_pszName, a_nIndex);
    return a_xMap.SetValueDeleted(strName);
}

//***********************************************************************
FETCH_VALUE_STRUCT* getValueByName(TSectionSegment &a_sb, LPCSTR a_pszName)
{
    s_pvFetchSeg = a_sb.FetchValueByName(a_pszName);
    int ofs = 0;

    if(s_pvFetchSeg)
    {
        do
        {
            FETCH_VALUE_STRUCT* pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, (void*)s_pvFetchSeg, ofs);
            ofs = pFetch->ofsNext;
            LPCSTR pszName = MAKE_PTR(LPCSTR, &pFetch->item, pFetch->item.iName);
            if(strcmp(pszName, a_pszName) == 0)
            {
                return pFetch;
            }
        }
        while(ofs);
    }
    return NULL;
}


//***********************************************************************
FETCH_VALUE_STRUCT* getValueById(TSectionSegment &a_sb, DWORD a_dwHash, WORD a_wOrder)
{
    s_pvFetchSeg = a_sb.FetchValueByID(a_dwHash, a_wOrder);
    int ofs = 0;

    do
    {
        FETCH_VALUE_STRUCT* pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, (void*)s_pvFetchSeg, ofs);
        ofs = pFetch->ofsNext;
        if(pFetch->item.dwHash == a_dwHash && pFetch->item.wOrder == a_wOrder)
        {
            return pFetch;
        }
    }
    while(ofs);

    return NULL;
}

//***********************************************************************
static bool deleteItem(TSectionSegment &a_sb, std::string &a_name)
{
    TValueMap xMap;
    xMap.SetValueDeleted(a_name.c_str());
    TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
    xMap.Clear();
    a_name.clear();
    return a_sb.ProcessBatch(pvBatch) == TRUE;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_AddDeleteMultipleItemsConsistency, "Add And Delete Multiple Items - check consistency")
{
    TSectionSegment sb;
    sb.Resize(1);

    TValueMap xMap;

    for(int i = 0; i < 5000; i++)
    {
        SetValue(xMap, "Wiek",     i, i);
        SetValue(xMap, "Nazwisko", i, "Pawe³ Bujak");
        SetValue(xMap, "Adres",    i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
    }

    int nCount = GetSectionCount(sb);
    TEST_VERIFY(nCount == 15000);

    // The goal of deleting odd and even separately is a better coverage (recycling entry)
    int i = 0;

    while(true)
    {
        TEST_VERIFY(SetValueDeleted(xMap, "Wiek", i));
        TEST_VERIFY(SetValueDeleted(xMap, "Nazwisko", i));
        TEST_VERIFY(SetValueDeleted(xMap, "Adres", i));

        i++;
        if((i % 160) == 0)
        {
            i += 400;
        }
        if(i >= 5000) break;

        if((i % 50) == 0)
        {
            TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
            xMap.Clear();
            TEST_VERIFY(sb.ProcessBatch(pvBatch));
        }
    }
    TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
    xMap.Clear();
    TEST_VERIFY(sb.ProcessBatch(pvBatch));

    TEST_VERIFY(ValidateSectionSegment(sb));

    return true;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_AddDeleteMultipleItems, "Add And Delete Multiple Items")
{
    TSectionSegment sb;
    sb.Resize(1);

    TValueMap xMap;

    for(int i = 0; i < 5000; i++)
    {
        SetValue(xMap, "Wiek",     i, i);
        SetValue(xMap, "Nazwisko", i, "Pawe³ Bujak");
        SetValue(xMap, "Adres",    i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
    }

    std::vector<std::string> names;
    GetSectionNames(names, sb);

    TEST_VERIFY(names.size() == 15000);

    // The goal of deleting odd and even separately is a better coverage (recycling entry)
    for(int i = 0; i < names.size(); i++)
    {
        if((i % 2) == 0)
        {
            TEST_VERIFY(deleteItem(sb, names[i]));
        }
    }
    for(int i = 0; i < names.size(); i++)
    {
        if(!names[i].empty())
        {
            TEST_VERIFY(deleteItem(sb, names[i]));
        }
    }

    int nCount = GetSectionCount(sb);

    TEST_VERIFY(nCount == 0);
    TEST_VERIFY(sb.GetPageCount() == 1);

    return true;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_AddDeleteItems, "Add And Delete Items")
{
    TSectionSegment sb;
    sb.Resize(1);

    std::set<CString> setDelNames;
    TValueMap         xMap;

    for(int i = 0; i < 1000; i++)
    {
        SetValue(xMap, "Age",     i, i);
        SetValue(xMap, "Name",    i, "Pawe³ Bujak");
        SetValue(xMap, "Address", i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
    }

    int nCount = GetSectionCount(sb);
    TEST_VERIFY(nCount == 3000);

    //--------------------------------------------------------------
    setDelNames.insert("Age 24");
    setDelNames.insert("Name 87");
    setDelNames.insert("Address 50");
    setDelNames.insert("Address 12");
    setDelNames.insert("Age 66");
    //--------------------------------------------------------------
    {
        for(std::set<CString>::const_iterator it = setDelNames.begin(); it != setDelNames.end(); ++it)
        {
            xMap.SetValueDeleted(*it);
        }
        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
    }
    //--------------------------------------------------------------
    std::vector<CString> vecNames;
    //--------------------------------------------------------------
    for(int i = 0; i < 1000; i++)
    {
        CString strName;
        strName.Format("Age %d", i);
        vecNames.push_back(strName);

        strName.Format("Name %d", i);
        vecNames.push_back(strName);

        strName.Format("Address %d", i);
        vecNames.push_back(strName);
    }
    //--------------------------------------------------------------
    for(std::vector<CString>::const_iterator it = vecNames.begin(); it != vecNames.end(); ++it)
    {
        CString strName = *it;

        if(setDelNames.find(strName) == setDelNames.end())
        {
            FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, strName);
            TEST_VERIFY(pFetch);

            LPCSTR pszName = MAKE_PTR(LPCSTR, &pFetch->item, pFetch->item.iName);
            TEST_VERIFY(strName == pszName);
        }
        else
        {
            FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, strName);
            TEST_VERIFY(pFetch == NULL); // delete succeeded
        }
    }
    
    SetValue(xMap, "Age", 55, 45);

    TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
    xMap.Clear();
    sb.ProcessBatch(pvBatch);

    return true;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_AddDeleteLittleCompact, "Add And Delete Little Elements And Compact")
{
    TSectionSegment sb;
    sb.Resize(1);

    std::set<CString> setDelNames;
    TValueMap         xMap;

    for(int i = 0; i < 100; i++)
    {
        SetValue(xMap, "A",   i, i);
        SetValue(xMap, "Name",  i, "Pawe³ Bujak");
        SetValue(xMap, "Address", i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
    }

    for(int i = 0; i < 20; i++)
    {
        CString strName;
        strName.Format("Name %d", i);
        xMap.SetValueDeleted(strName);
    }
    TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
    xMap.Clear();
    sb.ProcessBatch(pvBatch);


    return true;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_ModifyExistingItems, "Modify Existing Items")
{
//    Storage::TStorage      xStorage(FALSE);
    Storage::TAdvPageRef xRef(/*&xStorage*/);
    TSectionSegment sb;
    sb.Resize(1);

    std::set<CString> setDelNames;
    TValueMap         xMap;

    //----------------------------------------------------------------------------------------------
    for(int i = 0; i < 100; i++)
    {
        SetValue(xMap, "A",   i, i);
        SetValue(xMap, "Name",  i, "Pawe³ Bujak");
        SetValue(xMap, "Address", i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        TEST_VERIFY(sb.ProcessBatch(pvBatch));
    }

    //----------------------------------------------------------------------------------------------
    for(int i = 99; i >= 0; i--)
    {
        SetValue(xMap, "Address", i, i);
        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        TEST_VERIFY(sb.ProcessBatch(pvBatch));
    }
    //----------------------------------------------------------------------------------------------
    // Verify result
    TBASE_ITEM_FILTER filter = {0};
    strcpy(filter.szPattern, "Address *");

    std::vector<std::string> names;
    GetSectionNames(names, sb, &filter);
    TEST_VERIFY(names.size() == 100);

    for(int i = 0; i < 100; i++)
    {
        LPCSTR pszName = names[i].c_str();
        FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, pszName);
        int idxRef = 0;
        sscanf(pszName, "Address %d", &idxRef);
        int nType = TBITEM_TYPE(&pFetch->item);
        TEST_VERIFY(nType == TBVTYPE_INTEGER);

        int idx = (TBITEM_VALUE(&pFetch->item))->int32;
        TEST_VERIFY(idx == idxRef);
    }

    //----------------------------------------------------------------------------------------------
    // Greater value
    { 
        LPCSTR pszGreater = "Greater Value .............................................";

        SetValue(xMap, "A", 5, pszGreater);
        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        TEST_VERIFY(sb.ProcessBatch(pvBatch));

        FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, "A 5");
        TBVALUE_ENTRY *pValue = TBITEM_VALUE(&pFetch->item);
        TEST_VERIFY(strcmp(pValue->text, pszGreater) == 0);
    }
    //----------------------------------------------------------------------------------------------
    // Equal value
    {
        SetValue(xMap, "A", 10, 12);
        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        TEST_VERIFY(sb.ProcessBatch(pvBatch));

        FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, "A 10");
        TBVALUE_ENTRY *pValue = TBITEM_VALUE(&pFetch->item);
        TEST_VERIFY(pValue->int32 == 12);
    }    
    return true;
//RENAME
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_RenameItems, "Rename Items")
{
    TSectionSegment sb;
    sb.Resize(1);

    std::set<CString> setDelNames;
    TValueMap         xMap;

    //----------------------------------------------------------------------------------------------
    for(int i = 0; i < 100; i++)
    {
        SetValue(xMap, "A",   i, i);
        SetValue(xMap, "Name",  i, "Pawe³ Bujak");
        SetValue(xMap, "Address", i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");

        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        TEST_VERIFY(sb.ProcessBatch(pvBatch));
    }
    //----------------------------------------------------------------------------------------------
    for(int i = 0; i < 100; i++)
    {
        CString strKey, strKey2;

        strKey.Format("A %d", i);
        strKey2.Format("B %d", i);
        FETCH_VALUE_STRUCT *pFetch = getValueByName(sb, strKey);
        sb.ModifyValue((LPCSTR)strKey, strKey2, TBITEM_TYPE(&pFetch->item), TBITEM_VALUE(&pFetch->item), pFetch->item.wSize);

        strKey.Format("Name %d", i);
        strKey2 = strKey;
        strKey2.Append(" XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
        pFetch = getValueByName(sb, strKey);
        sb.ModifyValue((LPCSTR)strKey, strKey2, TBITEM_TYPE(&pFetch->item), TBITEM_VALUE(&pFetch->item), pFetch->item.wSize);
    }
    
    //----------------------------------------------------------------------------------------------
    return true;
}

//***********************************************************************
TEST_ENTRY(TSectionSegment_DirectWriteSmokeTest, "Direct Write Smoke Test")
{
    TSectionSegment sb;
    sb.Resize(1);

    std::set<CString> setDelNames;
    TValueMap         xMap;

    for(int i = 0; i < 500; i++)
    {
        SetValue(sb, "Age",     i, i);
        SetValue(sb, "Name",    i, "Pawe³ Bujak");
        SetValue(sb, "Address", i, "Wallisa 2/5 Bytom SSSSSSSSSSSSSSSSSSSSSSSS");
    }

    for(int i = 0; i < 500; i++)
    {
        SetValue(sb, "Age",     i, "AgeXXXXXXXXXXXXXXXX");
        SetValue(sb, "Name",    i, i*1);
        SetValue(sb, "Address", i, 10);
    }
    for(int i = 0; i < 500; i++)
    {
        CString strKey;
        strKey.Format("Name %d", i);
        sb.DeleteValue((LPCSTR)strKey);
    }
    sb.Compact();
    return true;
}

class TAuditHashSectionSegment: public TSectionSegment
{
public:
    bool Audit();
};

//***********************************************************************
TEST_ENTRY(TSectionSegment_LongHashLinkTest, "Long Hash Link Test")
{
   TAuditHashSectionSegment sb;
   sb.Resize(1);

   std::set<CString> setDelNames;
   TValueMap         xMap;

   char *pszName = "Ola";
   //----------------------------------------------------------------------------------------------
    for(int ctr = 0; ctr < 1500; ctr++)
    {
        CTBValueEntry val1(35);
        CString strName;
        strName.Format("%s:Age %d", pszName, ctr);
        xMap.SetValue(strName, val1.getType(), val1);

        CTBValueEntry val2("Any name");
        strName.Format("%s:Name %d", pszName, ctr);
        xMap.SetValue(strName, val2.getType(), val1);
 
        TTaskAutoPtr<void> pvBatch = xMap.ExportBatch();
        xMap.Clear();
        sb.ProcessBatch(pvBatch);
   }
   //----------------------------------------------------------------------------------------------
    return sb.Audit();
}

//***********************************************************************
bool TAuditHashSectionSegment::Audit()
{
    Storage::TAdvPageRef xRef;
    int                    effPageSize = COMMON_PageSizeAdjustedToFibonacci();

    //--------------------------------------------------------------------
    SECTION_HEADER *pHeaderSrc = (SECTION_HEADER *)GetPageForRead(0, xRef);
    TTaskAutoPtr<SECTION_HEADER> pHeader = (SECTION_HEADER *)TASK_MemAlloc(PAGESIZE);
    memcpy(pHeader.ptr(), pHeaderSrc, PAGESIZE);

    for(int i = 0; i < 241; i++)
    {
        int nItem = 0;
        BPOINTER bpElem = pHeader->m_pMap[i];
        while(bpElem)
        {
            Ref<TBITEM_ENTRY> itemP = GetItem(bpElem, xRef);
            DWORD dwHash2 = HASH_ComputeHash(TBITEM_NAME(itemP.ptr()));
            if((itemP->dwHash % 241) != i)
            {
                ASSERT(false);
                return false;
            };

            bpElem = itemP->bpNext;
            nItem++;
        }
    }
    //--------------------------------------------------------------------
    int nCount = GetPageCount();

    for(int i = 0; i < nCount; i++)
    {
        LPVOID pvData = GetPageForRead(i, xRef);
        int    nOffset = 0;

        while(nOffset < effPageSize)
        {
            TBITEM_ENTRY *pItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
            if(pItem->iType == TBVITYPE_NOTUSED)
                break;

            if (pItem->iType != TBVITYPE_DELETED && 
                pItem->iType != TBVITYPE_RESERVED)
            {
                LPCSTR pszName = TBITEM_NAME(pItem);
                DWORD dwHash2 = HASH_ComputeHash(pszName);
                BPOINTER bptr = FindValue(pszName);
                
                if(bptr == 0)
                {
                    ASSERT(false);
                    return false;
                }
            }
            ASSERT(pItem->wSize > 0);
            nOffset += (long)pItem->wSize;
        }
    }
    return true;
}
