#include "stdafx.h"

#if defined(UNIT_TEST)
#define MOCK_SEGMENT
#endif

#include "DataMgr.h"
#include "TTask.h"
#include "TBaseFile.h"
#include "Util.h"

#define MAX_EDIT_MODES 4

static long G_nMapSize     = -1;
static long G_nStartOffset = -1;

//**************************************************************
static BOOL InitMapSize()
{
    long ctr    = 0;
    long nSize  = 0;
    BOOL bPrime = FALSE;
    long fibBaseSize = COMMON_GetFibonacciBaseSize();

    G_nStartOffset = (PAGESIZE>>1);
    G_nStartOffset -= (G_nStartOffset % fibBaseSize);
    

    nSize = (G_nStartOffset - offsetof(SECTION_HEADER,m_pMap) - sizeof(WORD)) / sizeof(BPOINTER);

    do
    {
        bPrime = TRUE;
        for (ctr=2; ctr<nSize/2+1; ctr++)
        {
            if ((nSize % ctr)==0)
            {
                bPrime = FALSE;
                nSize--;
                break;
            }
        }
    }while (!bPrime);

    G_nMapSize = nSize;
    return TRUE;
}

//*************************************************************************************
inline static void UpdateItemFooter(TSectionSegment::Ref<TBITEM_ENTRY> &a_entry)
{
    TBITEM_FOOTER(a_entry.ptr()) = (WORD)a_entry.offset();
}

IMPLEMENT_INIT(InitMapSize);

TSemaphore TAdvancedSegment::s_semEditModeLimit(MAX_EDIT_MODES);

//============================================================
class TSectionSegmentCompact
{
    enum EMode {eFirstFit, eBestFit};

    Storage::TAdvPageRef m_xPageRef;

    TSectionSegment *m_pSegment;
    int            m_nTopPage;

    TBITEM_ENTRY *popEntryFromLastPage(int a_nCurPage, int a_cbSize);
    void          removeEmptyPages();
    bool          isPageEmpty(LPVOID a_pvPage);
public:
    inline TSectionSegmentCompact(TSectionSegment *a_pSegment): 
        m_pSegment(a_pSegment)
    {
        m_nTopPage = m_pSegment->GetPageCount() - 1;
    }
    bool compact(); 
};

//**************************************************************
TAdvancedSegment::~TAdvancedSegment()
{
    if(m_bEditMode)
        CancelEdit();
}

//**************************************************************
void* TAdvancedSegment::GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page)
{
    m_pageStateHash++;

    if(m_bEditMode)
    {
        return Edit_GetPageForRead(a_nIndex, a_page);
    }
    return TSegment::GetPageForRead(a_nIndex, a_page);
}

//**************************************************************
void* TAdvancedSegment::GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page)
{
    m_pageStateHash++;

    if(m_bEditMode)
    {
        return Edit_GetPageForWrite(a_nIndex, a_page);
    }
    return TSegment::GetPageForWrite(a_nIndex, a_page);
}

//**************************************************************
int TAdvancedSegment::GetPageCount()
{
    if (m_bEditMode)
    {
        if(m_nEditPageCount == -1)
            m_nEditPageCount = TSegment::GetPageCount();
        return m_nEditPageCount;
    }
    return TSegment::GetPageCount();
}

//**********************************************************
BOOL TAdvancedSegment::Resize(long a_nPageCount)
{
    long   ctr=0, nCount=0;
    LPVOID pvData = NULL;

    if (m_bEditMode)
    {
        m_nEditPageCount = a_nPageCount;

        POSITION pos = m_mapEditPages.GetStartPosition();
        //---------------------------------------------------
        while(pos)
        {
            int  nIndex = 0;
            void *pvData = NULL;
            m_mapEditPages.GetNextAssoc(pos, nIndex, pvData);
            if(nIndex >= a_nPageCount)
            {
                DataPage::TPagePool::getInstance().releaseBlock(pvData);
                m_mapEditPages.RemoveKey(nIndex);
            }
        }
        //---------------------------------------------------
        return TRUE;
    }
    return TSegment::Resize(a_nPageCount);
}

//**************************************************************
LPVOID TAdvancedSegment::Edit_GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_pageRef)
{
    void *pvData;
    if(!m_mapEditPages.Lookup(a_nIndex, pvData))
    {
        pvData = TSegment::GetPageForRead(a_nIndex, a_pageRef);
        if(pvData == NULL)
        {
            if(a_nIndex >= TSegment::GetPageCount())
            {
                pvData = DataPage::TPagePool::getInstance().getBlock();
                m_mapEditPages[a_nIndex] = pvData;
            }
        }
    }
    return pvData;
}

//**************************************************************
LPVOID TAdvancedSegment::Edit_GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_pageRef)
{
    void *pvData;
    if(!m_mapEditPages.Lookup(a_nIndex, pvData))
    {
        pvData = DataPage::TPagePool::getInstance().getBlock();
        if(a_nIndex < TSegment::GetPageCount())
        {
            void *pvSrc = TSegment::GetPageForRead(a_nIndex, a_pageRef);
            memcpy(pvData, pvSrc, PAGESIZE);
        }
        else
            memset(pvData, 0, PAGESIZE);

        m_mapEditPages[a_nIndex] = pvData;
    }
    return pvData;
}

//**************************************************************
void TAdvancedSegment::FreeEditData()
{
    POSITION pos = m_mapEditPages.GetStartPosition();
    //---------------------------------------------------
    while(pos)
    {
        int  nIndex = 0;
        void *pvData = NULL;
        m_mapEditPages.GetNextAssoc(pos, nIndex, pvData);

        DataPage::TPagePool::getInstance().releaseBlock(pvData);
    }
    //---------------------------------------------------
    m_mapEditPages.RemoveAll();
    m_nEditPageCount = -1;
}

//**************************************************************
BOOL TAdvancedSegment::Edit()
{
    if(!m_bEditMode)
    {
        s_semEditModeLimit.down();
        m_bEditMode = TRUE; 
        m_pageStateHash++;
    }
    return TRUE;
}

//**************************************************************
void TAdvancedSegment::CancelEdit()
{
    if (!m_bEditMode)
        return;

    FreeEditData();
    m_pageStateHash++;
    m_bEditMode = FALSE;
    s_semEditModeLimit.up();
}

//**************************************************************
BOOL TAdvancedSegment::Update()
{
    Storage::TAdvPageRef  xPageRef(m_ndbId);

    if (!m_bEditMode)
        return FALSE;

    if ((m_nEditPageCount != -1) && !TSegment::Resize(m_nEditPageCount))
    {
        CancelEdit();
        return FALSE;
    };
    POSITION pos = m_mapEditPages.GetStartPosition();
    //---------------------------------------------------
    while(pos)
    {
        int  nIndex = 0;
        void *pvData = NULL;
        m_mapEditPages.GetNextAssoc(pos, nIndex, pvData);
        
        void* pvTarget = TSegment::GetPageForWrite(nIndex, xPageRef);
        if(!pvTarget)
        {
            CancelEdit();
            return FALSE;
        }
        memcpy(pvTarget, pvData, PAGESIZE);
    }
    //---------------------------------------------------
    m_pageStateHash++;
    m_bEditMode = FALSE;
    FreeEditData();
    s_semEditModeLimit.up();

    return TRUE;
}

//*********************************************************
long TSectionSegment::CreateSegment(int a_ndbId, int a_segIdParent, DWORD a_dwAttrs, const Security::TSecurityAttributes &a_pSecAttrs)
{
    // do privileged
    long   nSegId = SEGMENT_Create(a_ndbId, a_pSecAttrs);
    BOOL   bReadOnly = FALSE;

    Storage::TAdvPageRef xPageRef(a_ndbId);

    TSegment seg;
    if(!SEGMENT_Assign(a_ndbId, nSegId, &seg))
        return 0;

    SECTION_HEADER *pHeader = (SECTION_HEADER *)seg.GetPageForWrite(0, xPageRef);
    pHeader->m_dwAttrs = a_dwAttrs;
    pHeader->m_header.wSize = (WORD)G_nStartOffset;
    pHeader->m_header.iType = TBVITYPE_RESERVED;
    TBITEM_FOOTER(&pHeader->m_header) = 0;

    TBITEM_ENTRY *pRest = MAKE_PTR(TBITEM_ENTRY *, pHeader, pHeader->m_header.wSize);
    pRest->wSize = (WORD)(COMMON_PageSizeAdjustedToFibonacci() - G_nStartOffset);
    pRest->iType = TBVITYPE_NOTUSED;
    TBITEM_FOOTER(pRest) = (WORD)G_nStartOffset;

    return nSegId;
}

//**********************************************************
TSectionSegment::~TSectionSegment()
{
}

//**********************************************************
TSectionSegment::Ref<TBITEM_ENTRY> TSectionSegment::GetFirstItemOfPage(int a_nPage, Storage::TAdvPageRef &a_xPageRef, EAccess e_access)
{
    return TSectionSegment::Ref<TBITEM_ENTRY>(a_xPageRef, this, BP_MAKEBPTR(a_nPage, 0), e_access);
}

//**********************************************************
TSectionSegment::Ref<TBITEM_ENTRY> TSectionSegment::GetItem(BPOINTER a_bpElem, Storage::TAdvPageRef &a_xPageRef, EAccess e_access)
{
    return TSectionSegment::Ref<TBITEM_ENTRY>(a_xPageRef, this, a_bpElem, e_access);
}

//**********************************************************
TSectionSegment::Ref<SECTION_HEADER> TSectionSegment::GetHeader(Storage::TAdvPageRef &a_xPageRef, EAccess e_access)
{
    return TSectionSegment::Ref<SECTION_HEADER>(a_xPageRef, this, 0, e_access);
}


//**********************************************************
BPOINTER TSectionSegment::FindValue(const TSectionSegment::ValueMatcher &a_valueKey)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);
    BPOINTER          bpElem = 0;
    DWORD             dwHash = a_valueKey.getHash();
    long              nIdx   = dwHash % G_nMapSize;  
    LPVOID            pvData = NULL;
    LPCSTR            pszName = NULL;
    BOOL              bSuitable = FALSE;
    long              nOffset   = 0; 

    Ref<SECTION_HEADER> headerP = GetHeader(xPageRef);

    bpElem = headerP->m_pMap[nIdx];

    while(bpElem)
    {
        Ref<TBITEM_ENTRY> entryP = GetItem(bpElem, xPageRef);

        if(a_valueKey.isMatching(entryP))
            return bpElem;

        ASSERT(bpElem != entryP->bpNext);
        bpElem = entryP->bpNext;
    }

    return 0;
};

//**********************************************************
void TSectionSegment::LinkEntry(TSectionSegment::Ref<TBITEM_ENTRY> & a_entryP)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    DWORD  dwHash  = a_entryP->dwHash;
    long   nIdx    = dwHash % G_nMapSize;

    Ref<SECTION_HEADER> headerP = GetHeader(xPageRef);
    WORD                wMaxOrder = 0;

    BPOINTER bpElem = headerP->m_pMap[nIdx];
    if(bpElem != 0)
    {
        Ref<TBITEM_ENTRY> itemP = GetItem(bpElem, xPageRef);
        wMaxOrder = itemP->wOrder;
    }
    headerP.makeWritable();
    headerP->m_pMap[nIdx] = a_entryP.bptr();

    a_entryP.makeWritable();
    a_entryP->bpNext = bpElem;
    a_entryP->wOrder = wMaxOrder + 1;
};

//**********************************************************
BPOINTER TSectionSegment::PrepareFreeEntry(WORD a_wNewSize)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);
    long               nLastPage = -1;
    BOOL               bSuitable = FALSE;
    BPOINTER           bpNew   = 0;
    
    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    //---------------------------------------------------------
    nLastPage = GetPageCount()-1;

    if (nLastPage==0)
    {
        Ref<TBITEM_ENTRY> itemP = GetFirstItemOfPage(nLastPage, xPageRef, eAccessWrite);
        if(itemP->iType == TBVITYPE_NOTUSED)
        {
            itemP->iType = TBVITYPE_RESERVED;
            itemP->wSize = (WORD)G_nStartOffset;
            UpdateItemFooter(itemP);

            itemP += itemP->wSize;
            itemP->wSize = (WORD)(effPageSize - itemP.offset());
            itemP->iType = TBVITYPE_NOTUSED;
            UpdateItemFooter(itemP);
        }
    }
    Ref<TBITEM_ENTRY> itemP = GetFirstItemOfPage(nLastPage, xPageRef, eAccessWrite);
    //---------------------------------------------------------
    do
    {
        if (itemP.offset() >= effPageSize)
        {
            nLastPage++;
            if (!Resize(nLastPage + 1))
                return 0;

            itemP = GetFirstItemOfPage(nLastPage, xPageRef, eAccessWrite);
            itemP->wSize = effPageSize; // Whole page as entry
            itemP->iType = TBVITYPE_NOTUSED;
            UpdateItemFooter(itemP);
            continue;
        }

        if(itemP->iType == TBVITYPE_NOTUSED || itemP->iType == TBVITYPE_DELETED)
        {
            if(itemP->wSize == a_wNewSize)
            {
                return itemP.bptr();
            }
            else if(itemP->wSize >= a_wNewSize)
            {
                Ref<TBITEM_ENTRY>  nextItemP(itemP, a_wNewSize);

                nextItemP->wSize = itemP->wSize - a_wNewSize;
                nextItemP->iType = itemP->iType;
                UpdateItemFooter(nextItemP);

                itemP->wSize = a_wNewSize;
                return itemP.bptr();
            }
        }

        ASSERT(itemP->wSize > 0);
        itemP += itemP->wSize;
    }while(1);
    //---------------------------------------------------------
    return 0;
}

//**********************************************************
TSectionSegment::Ref<TBITEM_ENTRY> TSectionSegment::GetFirstItem(TSectionSegment::Iterator &a_iter, Storage::TAdvPageRef &a_xPageRef, TSectionSegment::EAccess a_access, int a_nMinPage, int a_nMaxPage)
{
    a_iter.nMinPage = (a_nMinPage != -1 ? a_nMinPage : 0);
    a_iter.nMaxPage = (a_nMaxPage != -1 ? a_nMaxPage : GetPageCount() - 1);

    a_iter.nEffPageSize = COMMON_PageSizeAdjustedToFibonacci();
    a_iter.refP = GetFirstItemOfPage(a_iter.nMinPage, a_xPageRef, a_access);

    a_iter.pPageRef = &a_xPageRef;
    a_iter.eAccess = a_access;

    if(a_iter.refP->wSize == 0)
        return Ref<TBITEM_ENTRY>();

    return GetNextItem(a_iter);
}

//**********************************************************
TSectionSegment::Ref<TBITEM_ENTRY> TSectionSegment::GetNextItem(TSectionSegment::Iterator &a_iter)
{
    while(true)
    {
        if(a_iter.refP.offset() == a_iter.nEffPageSize)
        {
            int nPage = a_iter.refP.page();
            if(nPage >= a_iter.nMaxPage)
                return Ref<TBITEM_ENTRY>();

            a_iter.refP = GetFirstItemOfPage(++nPage, *a_iter.pPageRef, a_iter.eAccess);
        }

        TSectionSegment::Ref<TBITEM_ENTRY> itemP = a_iter.refP;
        a_iter.refP += a_iter.refP->wSize;

        if(itemP->iType != TBVITYPE_NOTUSED &&
           itemP->iType != TBVITYPE_DELETED &&
           itemP->iType != TBVITYPE_RESERVED)
        {
            return itemP;
        }
    }
    return Ref<TBITEM_ENTRY>();
}

//**********************************************************
TBITEM_ENTRY* TSectionSegment::CreateItemEntry(const TSectionSegment::NewValueKey &a_newValueKey, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    if(a_newValueKey.isAnonymous())
    {
        TBITEM_ENTRY* pEntry = (TBITEM_ENTRY*)COMMON_CreateItemEntry("", a_dwType, a_pValue, a_cbSize);
        pEntry->dwHash = a_newValueKey.getHash();
        pEntry->wOrder = a_newValueKey.getOrder();
        return pEntry;
    }
    else
    {
        LPCSTR pszName = a_newValueKey.getName();
        if (!pszName || !COMMON_CheckValueName(pszName))
        {
            TASK_SetErrorInfo(TRERR_INVALID_NAME);
            return NULL;
        }
        return (TBITEM_ENTRY*)COMMON_CreateItemEntry(pszName, a_dwType, a_pValue, a_cbSize);
    }
}


//**********************************************************
BPOINTER TSectionSegment::AddValue(const TSectionSegment::NewValueKey &a_newValueKey, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    Storage::TAdvPageRef       xPageRef(m_ndbId);
    BPOINTER                   bpNew   = 0;
    LPVOID                     pvData    = NULL; 
    Ref<TBITEM_ENTRY>          itemP;
    Util::TTaskAutoPtr<TBITEM_ENTRY> pNewItem = CreateItemEntry(a_newValueKey, a_dwType, a_pValue, a_cbSize);
    DWORD                      dwHash    = 0;
    long                       nIdx      = -1;  
    long                       nRetVal   = -1;
    long                       nOffset   = 0;
    long                       nLastPage = -1;
    BOOL                       bSuitable = FALSE;
    BOOL                       bExist    = FALSE;
    BOOL                       bDecrDelCount = FALSE;
    WORD                       wMaxOrder     = 0;

    if (pNewItem==NULL)
        return 0;

    //---------------------------------------------------------
    if(!a_newValueKey.isAnonymous())
    {
        BPOINTER bpElem = FindValue(a_newValueKey.getName());
        if (bpElem > 0) // exists
        {
            TASK_SetErrorInfo(TRERR_ALREADY_EXIST, a_newValueKey.getName());
            return 0;
        }
    }
    //---------------------------------------------------------
    //Find free
    bpNew = PrepareFreeEntry(pNewItem->wSize);
    itemP = GetItem(bpNew, xPageRef, eAccessWrite);

    if (itemP->iType == TBVITYPE_DELETED)
        bDecrDelCount = TRUE;

    memcpy(itemP.ptr(), pNewItem, pNewItem->wSize);
    itemP->bpNext = 0;
    UpdateItemFooter(itemP);
    //---------------------------------------------------------
    if (bpNew != 0)
    {
        LinkEntry(itemP);
    }
    //---------------------------------------------------------
    if (bDecrDelCount)
    {
        Ref<SECTION_HEADER> headerP = GetHeader(xPageRef, eAccessWrite);
        if (headerP)
            headerP->m_dwDelCount--;
    }
    return bpNew;
}

//**********************************************************
void TSectionSegment::RecycleEntry(TSectionSegment::Ref<TBITEM_ENTRY> & a_entryP)
{
    a_entryP.makeWritable();

    int nOffset = a_entryP.offset();

    a_entryP->iType = TBVITYPE_DELETED;

    if((nOffset + a_entryP->wSize) < (long)COMMON_PageSizeAdjustedToFibonacci())
    {
        Ref<TBITEM_ENTRY> nextP(a_entryP, a_entryP->wSize);

        if(nextP->iType == TBVITYPE_DELETED || nextP->iType == TBVITYPE_NOTUSED)
        {
            a_entryP->iType = nextP->iType;
            a_entryP->wSize += nextP->wSize;
            UpdateItemFooter(a_entryP);
        }
    }
    if(nOffset)
    {
        int nPrevOffset = *(WORD*)((BYTE*)a_entryP.ptr() - sizeof(WORD)); // get footer of previous
        int nDiff = nPrevOffset - nOffset;
        Ref<TBITEM_ENTRY> prevP(a_entryP, nDiff);

        if(prevP->iType == TBVITYPE_DELETED)
        {
            prevP->iType = a_entryP->iType;
            prevP->wSize += a_entryP->wSize;
            UpdateItemFooter(prevP);
        }
    }
}

//**********************************************************
BOOL TSectionSegment::DeleteValue(const TSectionSegment::ValueKey &a_valueKey)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    Ref<SECTION_HEADER> headerP;
    Ref<TBITEM_ENTRY>   entryP;
    BPOINTER         bpElem = 0;
    BPOINTER         bpPrev = 0;
    BPOINTER         bpNext = 0;
    DWORD            dwHash = a_valueKey.getHash();
    long             nIdx   = dwHash % G_nMapSize;  
    LPVOID           pvData = NULL;
    LPCSTR           pszName = NULL;
    BOOL             bSuitable = FALSE;
    long             nOffset   = 0; 
    BOOL             bFound    = FALSE;

    headerP = GetHeader(xPageRef);
    bpElem = headerP->m_pMap[nIdx];

    if (bpElem == 0)
        return 0;
    //--------------------------------------------------------------------
    while(bpElem)
    {
        entryP = GetItem(bpElem, xPageRef);
        if(a_valueKey.isMatching(entryP))
        {
            entryP.makeWritable();
            bFound = TRUE;
            bpNext = entryP->bpNext;
            RecycleEntry(entryP);
            break;
        };
        bpPrev = bpElem;
        bpElem = entryP->bpNext;
    }
    //--------------------------------------------------------------------
    if (!bFound)
    {
        TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, a_valueKey.getName());
        return 0;
    }

    if (bpPrev == 0)
    {
        headerP.makeWritable();
        headerP->m_pMap[nIdx] = bpNext;        
    }
    else
    {
        entryP = GetItem(bpPrev, xPageRef, eAccessWrite);
        entryP->bpNext = bpNext;
    }
    headerP.makeWritable();
    headerP->m_dwDelCount++;        

    return 1;
}

//**********************************************************
BPOINTER TSectionSegment::ModifyValue(const TSectionSegment::ValueKey &a_valueKey, LPCSTR a_pszNewName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    Storage::TAdvPageRef   xPageRef(m_ndbId);
    bool                   bUnlinked = false;

    Ref<SECTION_HEADER> headerP = GetHeader(xPageRef);
    
    BPOINTER bpElem = FindValue(a_valueKey);
    BPOINTER bpOld  = bpElem;
    //-------------------------------------------------------
    if (bpElem == 0)
    {   
        TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, a_valueKey.getName());
        return FALSE;
    }
    //-------------------------------------------------------
    if(a_pszNewName)
    {
        BPOINTER bpNewName = FindValue(a_pszNewName);
        if (bpNewName != 0)
        {
            TASK_SetErrorInfo(TRERR_ITEM_ALREADY_EXIST, a_pszNewName);
            return 0;
        }
        bUnlinked = true;
        Ref<TBITEM_ENTRY> entryP = GetItem(bpElem, xPageRef);
        UnlinkEntry(entryP);
    }
    //-------------------------------------------------------
    Util::TTaskAutoPtr<TBITEM_ENTRY> pNewItem = CreateItemEntry(a_pszNewName ? a_pszNewName : a_valueKey, a_dwType, a_pValue, a_cbSize);
    if (pNewItem == NULL)
        return FALSE;

    //------------------------------------------------
    Ref<TBITEM_ENTRY> itemP = GetItem(bpElem, xPageRef, eAccessWrite);

    pNewItem->wOrder = itemP->wOrder;
    pNewItem->bpNext = itemP->bpNext;

    // Zmie�ci si�
    if (pNewItem->wSize <= itemP->wSize)
    {
        if(pNewItem->wSize < itemP->wSize) 
        {
            Ref<TBITEM_ENTRY> nextItemP(itemP, pNewItem->wSize);

            int nNextSize = itemP->wSize - pNewItem->wSize;
            memset(nextItemP.ptr(), 0, nNextSize);
            nextItemP->wSize = nNextSize;
            TBITEM_FOOTER(nextItemP.ptr()) = WORD(nextItemP.offset());

            itemP->wSize = pNewItem->wSize;
            UpdateItemFooter(itemP);

            RecycleEntry(nextItemP);
            
            headerP.makeWritable();
            headerP->m_dwDelCount++;
        }

        TBITEM_FOOTER(pNewItem.ptr()) = itemP.offset();
        memcpy(itemP.ptr(), pNewItem, itemP->wSize);
    }
    //-------------------------------------------------------
    // Wi�kszy
    else
    {
        if(!bUnlinked)
        {
            bUnlinked = true;
            UnlinkEntry(itemP);
        }
        RecycleEntry(itemP);

        //-------------------------------------------
        // Pobierz nowy element
        bpElem = PrepareFreeEntry(pNewItem->wSize);
        itemP = GetItem(bpElem, xPageRef, eAccessWrite);
        memcpy(itemP.ptr(), pNewItem, pNewItem->wSize);
        UpdateItemFooter(itemP);
    }
    //-------------------------------------------------------
    if(a_pszNewName) // Full relink
    {
        LinkEntry(itemP);
    }
    else if(bUnlinked)
    {
        RelinkEntry(itemP);
    }
    return bpElem;
};

//**********************************************************
BOOL TSectionSegment::Compact()
{
    TSectionSegmentCompact compact(this);
    return compact.compact();
/*
    Storage::TAdvPageRef  xPageRef;
    SECTION_HEADER         *pHeader   = NULL;
    TBITEM_ENTRY           *pItem     = NULL;
    TBITEM_ENTRY           *pTmpItem  = NULL;
    TBITEM_ENTRY           *pDestItem = NULL;
    long                    nLastPage = GetPageCount()-1;
    long                    nPage     = 0;
    long                    nPage1    = 0;
    long                    nOffset   = G_nStartOffset;
    long                    nOffset1  = G_nStartOffset;
    LPVOID                  pvData    = NULL;
    BOOL                    bMove     = FALSE;
    DWORD                   dwHash    = 0;
    long                    nIdx      = 0;
    BPOINTER                bpFirst   = 0;
    BPOINTER                bpPrev    = 0;
    BOOL                    bExist    = FALSE;
    WORD                    wMaxOrder = 0;
    long                    nSize     = 0;
    BOOL                    bSkip     = FALSE;
    BOOL                    bEOP      = FALSE;
    CString                 strName;
    
    pHeader = (SECTION_HEADER*)GetPageForWrite(0, xPageRef);
    if (pHeader==NULL)
        return FALSE;
    memset(pHeader, 0, G_nStartOffset);
    //--------------------------------------------------
    do
    {   
        pvData = GetPageForWrite(nPage, xPageRef);
        if (pvData==NULL)
            return FALSE;
        pItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
        //----------------------------
        bEOP =  (PAGESIZE - nOffset) < sizeof(TBITEM_ENTRY);
        bEOP |= (pItem->iType == TBVITYPE_NOTUSED);
        if (bEOP)
        {
            if (nPage==nLastPage)
                break;
            nPage++;
            nOffset = 0;
            continue;
        }
        nSize = pItem->wSize;
        nOffset += nSize;
        bSkip  = FALSE;
        //----------------------------
        if (pItem->iType == TBVITYPE_DELETED)
        {
            bSkip = TRUE;
            bMove = TRUE;
        }
        //----------------------------
        strName = TBITEM_NAME(pItem);
        dwHash = HASH_ComputeHash(strName);
        nIdx   = dwHash % G_nMapSize;
        //----------------------------
        if (!bMove)
        {
            nOffset1 = nOffset;
            nPage1   = nPage;
            pItem->bpNext = 0;
        }
        else if(!bSkip)
        {
            //----------------------------------------------------
            pTmpItem = (TBITEM_ENTRY*)TASK_MemAlloc(nSize);
            if (pTmpItem==NULL)
                return FALSE;
            memcpy(pTmpItem, pItem, nSize);
            pTmpItem->bpNext = 0;

            if (nOffset1 + nSize >= (PAGESIZE - sizeof(TBITEM_ENTRY)))
            {
                pvData = GetPageForWrite(nPage1, xPageRef);
                if (pvData==NULL)
                    return FALSE;
                memset(MAKE_PTR(BYTE*, pvData, nOffset1), 0, PAGESIZE-nOffset1);
                nPage1++;
                nOffset1 = 0;
            }

            //----------------------------------------------------
            pvData = GetPageForWrite(nPage1, xPageRef);
            if (pvData==NULL)
                return FALSE;
            pDestItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset1);
            memcpy(pDestItem, pTmpItem, nSize);
            TASK_MemFree(pTmpItem);
            nOffset1 += nSize;
        }
        //----------------------------------------------------
        // Uaktualnij strukture ��czy
        if (!bSkip)
        {
            pHeader = (SECTION_HEADER*)GetPageForWrite(0, xPageRef);
            if (pHeader==NULL)
                return FALSE;
            bpFirst = pHeader->m_pMap[nIdx];
            if (bpFirst>0)
            {
                bpPrev = GetLastElem(dwHash, bpFirst, strName, bExist, wMaxOrder);
                if (bExist)
                    return FALSE;
            }
            LinkEntry(nIdx, BP_MAKEBPTR(nPage1, nOffset1-nSize), bpFirst, bpPrev);
        }
    }
    while(1);
    //--------------------------------------------------
    pvData = GetPageForWrite(nPage1, xPageRef);
__unnamed_00ad_2    if (pvData==NULL)
        return FALSE;
    memset(MAKE_PTR(BYTE*, pvData, nOffset1), 0, PAGESIZE-nOffset1);
    Resize(nPage1+1);

    return TRUE;
*/
}

//**********************************************************
LPVOID TSectionSegment::FetchValueByName(LPCSTR a_pszName, BOOL a_bSkipSections)
{
    Storage::TAdvPageRef  xPageRef(m_ndbId);
    BPOINTER                bpElem  = FindValue(a_pszName);
    LPVOID                  pvData  = NULL;
    long                    nOffset = 0;  
    TBITEM_ENTRY           *pItem   = NULL;

    if (bpElem==0)
    {
        TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, a_pszName);
        return NULL;
    }

    pvData = GetPageForRead(BP_PAGE(bpElem), xPageRef);
    nOffset = BP_OFS(bpElem);

    if (!pvData)
    {
        return NULL;
    }
    pItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);

    if (a_bSkipSections && TBITEM_TYPE(pItem)==TBVTYPE_SECTION)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName);
        return NULL;
    }

    return FetchValue(bpElem, a_bSkipSections);
}

//**********************************************************
LPVOID TSectionSegment::FetchValueByID(DWORD a_dwHash, WORD a_wOrder)
{
    BPOINTER  bpElem = FindValue(ValueKey(a_dwHash, a_wOrder));
    return FetchValue(bpElem);
}


//**********************************************************
LPVOID TSectionSegment::FetchValue(BPOINTER a_bpElem, BOOL a_bSkipSections)
{
    int                     effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    Storage::TAdvPageRef  xPageRef(m_ndbId);
    long                    nSegSize = (PAGESIZE * 5) / 4;
    long                    nSize    = 0;
    FETCH_VALUE_STRUCT     *pFetch   = NULL;
    TBITEM_ENTRY           *pItem    = NULL;
    long                    nOffset  = 0;
    long                    nOffset1 = 0;
    long                    nPrevOffset1 = -1;
    LPVOID                  pvData       = NULL;   
    LPVOID                  pvFetchSeg   = NULL;       
    BOOL                    bSkip        = FALSE;

    if (a_bpElem==NULL)
        return NULL;

    pvFetchSeg = TASK_MemAlloc(nSegSize);
    pvData = (TBITEM_ENTRY*)GetPageForRead(BP_PAGE(a_bpElem), xPageRef);

    if (pvData==NULL)
    {
        return NULL;
    }
    //-----------------------------------------------------
    while(nOffset < effPageSize)
    {
        pItem = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
        if (pItem->iType == TBVITYPE_NOTUSED)
            break;

        nSize = pItem->wSize;
        ASSERT(nSize > 0);        
        nOffset += nSize;

        bSkip = (pItem->iType == TBVITYPE_DELETED || pItem->iType == TBVITYPE_RESERVED);
        if (a_bSkipSections)
        {
            bSkip = bSkip || (pItem->iType == TBVTYPE_SECTION); 
        }
        if (!bSkip)
        {
            pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, pvFetchSeg, nOffset1);
            pFetch->ofsNext = 0;
            memcpy(&pFetch->item, pItem, nSize);

            if (nPrevOffset1 != -1)
            {
                pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, pvFetchSeg, nPrevOffset1);
                pFetch->ofsNext = nOffset1;
            }

            nPrevOffset1 = nOffset1;
            nOffset1 += nSize + offsetof(FETCH_VALUE_STRUCT, item);
        }
    };
    //-----------------------------------------------------
    return pvFetchSeg;
}

//**********************************************************
BOOL TSectionSegment::ProcessBatch(LPVOID a_pvBatch, BOOL a_bCanCompact)
{
    Storage::TAdvPageRef  xPageRef(m_ndbId);
    TB_BATCH_ENTRY         *pBatchEntry = (TB_BATCH_ENTRY *)a_pvBatch;
    long                    nOffset     = 0;
    long                    nPageOffset = 0;
    LPCSTR                  pszName     = NULL; 
    DWORD                   dwType      = 0;
    TBVALUE_ENTRY          *pValEntry   = NULL;
    TBITEM_ENTRY           *pItem       = NULL;
    TBITEM_ENTRY           *pFoundItem  = NULL;
    BPOINTER                bpElem      = 0;
    LPVOID                  pvPage      = NULL;
    SECTION_HEADER         *pHeader     = NULL;
    BOOL                    bRetVal     = FALSE;
    int                     storageFlags = (xPageRef.getStorage() ? xPageRef.getStorage()->getFlags() : 0);

    if (!Edit())
        return FALSE;
    //---------------------------------------------------------
    do
    {
        pItem = &pBatchEntry->item;
        pszName   = TBITEM_NAME(pItem);            
        dwType    = TBITEM_TYPE(pItem);
        pValEntry = TBITEM_VALUE(pItem);

        // Only deleting allowed if media full;
        if((pBatchEntry->action != eActionDel) && (storageFlags & Storage::eFlag_DeleteOnly) )
        {
            CancelEdit();
            TASK_SetErrorInfo(TRERR_DISK_FULL, pszName, NULL);
            return FALSE;
        }

        bpElem = FindValue(pszName);
        if (bpElem!=0)
        {
            pvPage = GetPageForRead(BP_PAGE(bpElem), xPageRef);
            nPageOffset = BP_OFS(bpElem); 

            if (pvPage==NULL)
            {
                CancelEdit();
                return FALSE;
            }
            pFoundItem = MAKE_PTR(TBITEM_ENTRY*, pvPage, nPageOffset);
            DWORD dwFoundType = TBITEM_TYPE(pFoundItem);

            // Batch nie mo�e zmieni� podsekcji ani BLOBu
            if (dwFoundType==TBVTYPE_SECTION || dwFoundType==TBVTYPE_LONGBINARY)
            {
                CancelEdit();
                TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, pszName, NULL);
                return FALSE;
            }
            if (pBatchEntry->action == eActionDel)
            {
                DeleteValue(pszName);
            }
            else
            {
                bpElem = ModifyValue(pszName, NULL, dwType, pValEntry, pBatchEntry->cbSize);
                if (bpElem==0)
                {
                    CancelEdit();
                    return FALSE;
                }
            }
        }
        else
        {
            if (pBatchEntry->action == eActionDel)
            {
                CancelEdit();
                TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, pszName, NULL);
                return FALSE;
            }
            bpElem = AddValue(pszName, dwType, pValEntry, pBatchEntry->cbSize);
            if (bpElem==0)
            {
                CancelEdit();
                return FALSE;
            }
        }

        nOffset = pBatchEntry->ofsNext;
        if (nOffset!=0)
        {
            pBatchEntry = MAKE_PTR(TB_BATCH_ENTRY*, a_pvBatch, nOffset);
        }
    }while(nOffset!=0);
    //---------------------------------------------------------
    pvPage = GetPageForRead(0, xPageRef);
    if (pvPage==NULL)
    {
        CancelEdit();
        return FALSE;
    }

    pHeader = (SECTION_HEADER*)pvPage;
    if (pHeader->m_dwDelCount > MAXDELETED && a_bCanCompact)
        Compact();

    bRetVal = Update();
    return bRetVal;
}

//**********************************************************
BOOL TSectionSegment::IsEmpty(BOOL &a_bEmpty)
{
    Storage::TAdvPageRef  xPageRef(m_ndbId);
    long                  ctr     = 0;

    Ref<SECTION_HEADER> headerP = GetHeader(xPageRef);

    a_bEmpty = TRUE;

    for (ctr=0; ctr<G_nMapSize; ctr++)
    {
        if (headerP->m_pMap[ctr]!=0)
        {
            a_bEmpty = FALSE;
            break;
        }
    }
    return TRUE;
}

//**********************************************************
DWORD TSectionSegment::GetDelCount()
{
    Storage::TAdvPageRef  xPageRef(m_ndbId);

    Ref<SECTION_HEADER> headerP = GetHeader(xPageRef);

    return headerP->m_dwDelCount;;
}

//**********************************************************
bool TSectionSegment::ValueKey::isMatching(TBITEM_ENTRY *a_pEntry) const
{
    switch(m_type)
    {
        case eByName:
        {
            if (a_pEntry->dwHash == getHash())
            {
                LPCSTR pszName = TBITEM_NAME(a_pEntry);
                if (stricmp(m_strName, pszName)==0)
                    return true;
            }
            break;
        }
        case eByID:
        {
            return (getHash() == a_pEntry->dwHash) && (m_wOrder == a_pEntry->wOrder);
        }
        default:
            ASSERT(!"Invalid type");
    }
    return false;
}

//**********************************************************
LPCSTR TSectionSegment::ValueKey::getName() const
{
    if(m_type == eByName)
    {
        return m_strName;
    }
    return "";
}

//**********************************************************
void TSectionSegment::UnlinkEntry(TSectionSegment::Ref<TBITEM_ENTRY> & a_entryP)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    long                nIdx = a_entryP->dwHash % G_nMapSize;  
    Ref<SECTION_HEADER> hdrP = GetHeader(xPageRef);

    if(hdrP->m_pMap[nIdx] == a_entryP.bptr())
    {
        hdrP.makeWritable();
        hdrP->m_pMap[nIdx] = a_entryP->bpNext;
        a_entryP.makeWritable();
        a_entryP->bpNext = 0;
        return;
    }

    BPOINTER bpThis = hdrP->m_pMap[nIdx];

    while(bpThis != 0)
    {
        Ref<TBITEM_ENTRY> entryP = GetItem(bpThis, xPageRef);
        if(entryP->bpNext == a_entryP.bptr())
        {
            entryP.makeWritable();
            entryP->bpNext = a_entryP->bpNext;
            
            a_entryP.makeWritable();
            a_entryP->bpNext = 0;
            return;
        }
        bpThis = entryP->bpNext;
    }
}

//**********************************************************
void TSectionSegment::RelinkEntry(TSectionSegment::Ref<TBITEM_ENTRY> & a_entryP)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    long                nIdx = a_entryP->dwHash % G_nMapSize;  
    Ref<SECTION_HEADER> hdrP = GetHeader(xPageRef);

    BPOINTER bpElem = hdrP->m_pMap[nIdx], bpPrev = 0;
    
    if(bpElem == 0)
    {
        hdrP.makeWritable();
        a_entryP.makeWritable();

        hdrP->m_pMap[nIdx] = a_entryP.bptr();
        a_entryP->bpNext = 0;
        return;
    }
    while(bpElem)
    {
        Ref<TBITEM_ENTRY> itemP = GetItem(bpElem, xPageRef);

        if(itemP->wOrder < a_entryP->wOrder)
        {
            a_entryP.makeWritable();
            a_entryP->bpNext = bpElem;
            if(bpPrev == 0)
            {
                hdrP.makeWritable();
                hdrP->m_pMap[nIdx] = a_entryP.bptr();
            }
            else
            {
                Ref<TBITEM_ENTRY> prevItemP = GetItem(bpPrev, xPageRef, eAccessWrite);
                prevItemP->bpNext = a_entryP.bptr();
            }
            return;
        }

        bpPrev = bpElem;
        bpElem = itemP->bpNext;
    }

    // was last
    a_entryP.makeWritable();
    a_entryP->bpNext = 0;
    Ref<TBITEM_ENTRY> prevItemP = GetItem(bpPrev, xPageRef, eAccessWrite);
    prevItemP->bpNext = a_entryP.bptr();
}

//**********************************************************
ITERATED_ITEM_ENTRY* TSectionSegmentIterator::getNextItems(TSectionSegment* a_pSegment)
{
    Storage::TAdvPageRef xPageRef(a_pSegment->m_ndbId);

    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    int nPageCount  = a_pSegment->GetPageCount();

    for( ; m_nPage < nPageCount; ++m_nPage)
    {
        void* pvBuff = a_pSegment->GetPageForRead(m_nPage, xPageRef);

        while(m_nOffset < effPageSize)
        {
            TBITEM_ENTRY *pEntry = MAKE_PTR(TBITEM_ENTRY*, pvBuff, m_nOffset);

            if(pEntry->iType == TBVITYPE_NOTUSED)
                break;

            if(pEntry->iType != TBVITYPE_DELETED && pEntry->iType != TBVITYPE_RESERVED)
            {
                BOOL bSuitable = TRUE;
                if (m_filter.IsFilter())
                {
                    LPCSTR pszName = TBITEM_NAME(pEntry);
                    DWORD dwType  = TBITEM_TYPE(pEntry);
                    bSuitable = m_filter.IsSuitable(pszName, dwType);
                }
                if (bSuitable)
                {
                    if(!pushEntry(pEntry))
                        return getResult(true);
                }
            }
            m_nOffset += pEntry->wSize;
        }

        m_nOffset = 0;
    }

    m_eof = true;
    return getResult(false);
}

//**********************************************************
bool TSectionSegmentIterator::pushEntry(TBITEM_ENTRY *a_pEntry)
{
    if(m_pvResult == NULL)
    {
        m_pvResult = TASK_MemAlloc(eBLOCK_SIZE);
        m_nResultOffset = 0;
    }

    LPCSTR pszName = TBITEM_NAME(a_pEntry);
    int    resSize = (sizeof(ITERATED_ITEM_ENTRY) + strlen(pszName) + 3) & ~3;

    if((m_nResultOffset + resSize + eBLOCK_MARGIN) > eBLOCK_SIZE)
        return false;

    ITERATED_ITEM_ENTRY *pTarget = MAKE_PTR(ITERATED_ITEM_ENTRY*, m_pvResult, m_nResultOffset);

    pTarget->wSize = resSize;
    pTarget->id.dwHash = a_pEntry->dwHash;
    pTarget->id.wOrder = a_pEntry->wOrder;
    pTarget->dwType = TBITEM_TYPE(a_pEntry);
    strcpy(pTarget->szName, pszName);

    m_nResultOffset += resSize;

    return true;
}

//**********************************************************
ITERATED_ITEM_ENTRY* TSectionSegmentIterator::getResult(bool a_hasMore)
{
    if(m_pvResult == NULL)
        m_pvResult = TASK_MemAlloc(eBLOCK_MARGIN);

    ITERATED_ITEM_ENTRY *pTarget = MAKE_PTR(ITERATED_ITEM_ENTRY*, m_pvResult, m_nResultOffset);
    pTarget->wSize = a_hasMore ? ITERATOR_END_BLOCK : ITERATOR_END_ALL;

    pTarget = (ITERATED_ITEM_ENTRY*)m_pvResult;

    m_pvResult = 0;
    m_nResultOffset = 0;

    return pTarget;
}

//**********************************************************
TBITEM_ENTRY *TSectionSegmentCompact::popEntryFromLastPage(int a_nCurPage, int a_cbSize)
{
    Storage::TAdvPageRef xPageRef(m_pSegment->m_ndbId);

    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();

    TSectionSegment::Ref<TBITEM_ENTRY> entryP = m_pSegment->GetFirstItemOfPage(m_nTopPage, xPageRef);

    if(isPageEmpty(entryP.ptr()))
    {
        if((--m_nTopPage) <= a_nCurPage)
            return NULL;
        entryP = m_pSegment->GetFirstItemOfPage(m_nTopPage, xPageRef);
    }

    int modes[] = {eBestFit, eFirstFit};

    for(int imode = 0; imode < sizeof(modes) / sizeof(modes[0]); imode++)
    {
        int mode = modes[imode];

        entryP = m_pSegment->GetFirstItemOfPage(m_nTopPage, xPageRef);

        while(entryP.offset() < effPageSize)
        {
            if(entryP->iType != TBVITYPE_DELETED && entryP->iType != TBVITYPE_NOTUSED)
            {
                if(mode == eBestFit ? a_cbSize == entryP->wSize : a_cbSize >= entryP->wSize)
                {
                    TBITEM_ENTRY *pNewEntry = (TBITEM_ENTRY *)TASK_MemAlloc(entryP->wSize);
                    memcpy(pNewEntry, entryP.ptr(), entryP->wSize);
                    m_pSegment->UnlinkEntry(entryP);
                    m_pSegment->RecycleEntry(entryP);
                    return pNewEntry;
                }
            }
            entryP += entryP->wSize;
        }
    }
    return NULL;
}

//**********************************************************
bool TSectionSegmentCompact::compact()
{
    Storage::TAdvPageRef xPageRef(m_pSegment->m_ndbId);

    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    int nCurPage = 0;
    int nOffset = 0;
    bool bWritable = false;
    TSectionSegment::Ref<TBITEM_ENTRY> entryP = m_pSegment->GetFirstItemOfPage(0, xPageRef);

    while(nCurPage < m_nTopPage)
    {
        int cbSize = entryP->wSize;
        ASSERT(cbSize > 0);    

        if(entryP->iType == TBVITYPE_DELETED || entryP->iType == TBVITYPE_NOTUSED)
        {
            if(!bWritable)
            {
                bWritable = true;
                entryP.makeWritable();
            }

            Util::TTaskAutoPtr<TBITEM_ENTRY> pPopEntry = popEntryFromLastPage(nCurPage, cbSize);
            while(pPopEntry && cbSize > 0)
            {
                memcpy(entryP.ptr(), pPopEntry.ptr(), pPopEntry->wSize);
                UpdateItemFooter(entryP);
                m_pSegment->RelinkEntry(entryP);
                cbSize -= pPopEntry->wSize;
                entryP += pPopEntry->wSize;
                pPopEntry = cbSize ? popEntryFromLastPage(nCurPage, cbSize) : NULL;
            }
            if(cbSize > 0)
            {
                entryP->wSize = cbSize;
                entryP->iType = TBVITYPE_DELETED;
                TBITEM_FOOTER(entryP.ptr()) = entryP.offset();
            }
        }
        
        entryP += cbSize;
        if(entryP.offset() >= effPageSize)
        {
            bWritable = false;
            entryP = m_pSegment->GetFirstItemOfPage(++nCurPage, xPageRef);
        }
        m_xPageRef.releaseAll();
    }

    removeEmptyPages();

    TSectionSegment::Ref<SECTION_HEADER> hdrP = m_pSegment->GetHeader(m_xPageRef, TSectionSegment::eAccessWrite);
    hdrP->m_dwDelCount = 0;

    return true;
}

//**********************************************************
void TSectionSegmentCompact::removeEmptyPages()
{
    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    int nLastPage = m_pSegment->GetPageCount() - 1;
    LPVOID pvData = m_pSegment->GetPageForRead(nLastPage, m_xPageRef);
    bool   bShrink = false;
    while(isPageEmpty(pvData))
    {
        pvData = m_pSegment->GetPageForRead(--nLastPage, m_xPageRef);
        bShrink = true;
    }
    if(!bShrink)
        return;

    m_pSegment->Resize(nLastPage + 1);

    // modify last segment
    pvData = m_pSegment->GetPageForWrite(nLastPage, m_xPageRef);
    int nOffset = *MAKE_PTR(WORD*, pvData, effPageSize - sizeof(WORD)); // get offset from footer
    
    TBITEM_ENTRY *pEntry = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
    if(pEntry->iType == TBVITYPE_DELETED)
        pEntry->iType = TBVITYPE_NOTUSED;
}

//**********************************************************
bool TSectionSegmentCompact::isPageEmpty(LPVOID a_pvPage)
{
    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();
    TBITEM_ENTRY *pEntry = (TBITEM_ENTRY *)a_pvPage;

    return (pEntry->wSize == effPageSize && 
              (pEntry->iType == TBVITYPE_DELETED || pEntry->iType == TBVITYPE_NOTUSED)
           );
}

//************************************************************************************
bool Util::Segment::readPage(TSegment* a_pSegment, int a_nPage, void *a_pvBuff)
{
    Storage::TAdvPageRef xPageRef(a_pSegment->m_ndbId);

    if(a_nPage >= a_pSegment->GetPageCount())
        return false;

    void *pvBuff = a_pSegment->GetPageForRead(a_nPage, xPageRef);
    memcpy(a_pvBuff, pvBuff, PAGESIZE);

    return true;
}

//************************************************************************************
bool Util::Segment::writePage(TSegment* a_pSegment, int a_nPage, void *a_pvBuff)
{
    Storage::TAdvPageRef xPageRef(a_pSegment->m_ndbId);

    if(a_nPage >= a_pSegment->GetPageCount())
        a_pSegment->Resize(a_nPage + 1);

    void *pvBuff = a_pSegment->GetPageForWrite(a_nPage, xPageRef);
    memcpy(pvBuff, a_pvBuff, PAGESIZE);

    return true;
}

