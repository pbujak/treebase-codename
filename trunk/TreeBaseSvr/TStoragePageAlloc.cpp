#include "stdafx.h"
#include "TStoragePageAlloc.h"
#include "TBaseFile.h"

#define GET_PAGE_COUNT 8

using namespace Storage;

static TSpinLock G_slPageMgr;

TThreadLocal<TAdvPageRef::Data*> TAdvPageRef::Data::s_pData;

//**********************************************************
TAdvPageRef::Data* TAdvPageRef::Data::getInstance()
{
    if(s_pData.Value() != NULL)
    {
        Data *pData = s_pData.Value();
        pData->m_nRefCount++;
        return pData;
    }
    else
    {
        Data *pData = new Data();
        pData->m_nRefCount = 1;
        s_pData = pData;
        return pData;
    }
}

//**********************************************************
void TAdvPageRef::Data::releaseAllPages()
{
    m_pStorage->releasePage(m_pageRef);

    TPageRefMapIter iter;
    
    for(iter = m_guardPageRefs.begin(); iter != m_guardPageRefs.end(); ++iter)
    {
        DataPage::TPageRef &ref = iter->second;
        m_pStorage->releasePage(ref);
    }
    m_guardPageRefs.clear();
}

//**********************************************************
void TAdvPageRef::Data::saveState(SPageState &a_state)
{
//    if(m_nRefCount <= 1)
//        return;

    a_state.nAccess    = m_pageRef.getAccess();
    a_state.nStateHash = m_nStateHash;
    a_state.pStorage   = m_pStorage;

    if(a_state.nAccess != DataPage::TPageRef::eNoAccess)
    {
        a_state.fpPage = m_pageRef.getPage();
        m_pageRef.lockDiscard(+1);
    }
}

//**********************************************************
void TAdvPageRef::Data::restoreState(SPageState &a_state)
{
//    if(m_nRefCount <= 1)
//        return;

    if(m_pStorage != a_state.pStorage)
    {
        m_pStorage->releasePage(m_pageRef);
        m_pStorage = a_state.pStorage;
    }

    switch(a_state.nAccess)
    {
        case DataPage::TPageRef::eRead:
            m_pStorage->getPageForRead(a_state.fpPage, m_pageRef);
            m_pageRef.lockDiscard(-1);
            break;
        case DataPage::TPageRef::eReadWrite:
            m_pStorage->getPageForWrite(a_state.fpPage, m_pageRef);
            m_pageRef.lockDiscard(-1);
            break;
    }
    m_nStateHash = a_state.nStateHash;
}

//**********************************************************
void TAdvPageRef::Data::endTrans()
{
    for(std::set<TStorage*>::const_iterator it = m_setTrans.begin(); it != m_setTrans.end(); ++it)
    {
        (*it)->endTrans();
    }
    m_setTrans.clear();
}

//**********************************************************
TAdvPageRef::Data::~Data()
{
    endTrans();
}

//**********************************************************
void TAdvPageRef::Data::release()
{
    if((--m_nRefCount) == 0)
    {
        s_pData.Value() = NULL;
        delete this;
    }
}

//**********************************************************
void TAdvPageRef::Data::beginTrans()
{
    if(m_setTrans.find(m_pStorage) == m_setTrans.end())
    {
        m_setTrans.insert(m_pStorage);
        m_pStorage->beginTrans();
    }
}

//**********************************************************
TAdvPageRef::TAdvPageRef()
{
    ASSERT(Data::s_pData.Value() != NULL);
    init(Data::s_pData.Value()->m_pStorage);
}

//**********************************************************
TAdvPageRef::TAdvPageRef(int a_ndbId)
{
    TStorage *pStorage = G_dbManager.GetFile(a_ndbId);
    init(pStorage);
}

//**********************************************************
void TAdvPageRef::init(TStorage *a_pStorage)
{
    m_pData = Data::getInstance();
    m_pStorage = a_pStorage;

    m_pData->saveState(m_prevState);
    m_pData->m_pStorage = a_pStorage;
}

//**********************************************************
TAdvPageRef::~TAdvPageRef()
{
    typedef std::set<FPOINTER>::iterator Iterator;
    for(Iterator it = m_setLockedPages.begin(); it != m_setLockedPages.end(); it++)
    {
        FPOINTER fpPage = *it;
        DataPage::TPageRef &refPage = m_pData->m_guardPageRefs[fpPage];
        m_pStorage->releasePage(refPage);
        m_pData->m_guardPageRefs.erase(fpPage);
    }

    m_pData->restoreState(m_prevState);
    m_pData->release();
}

//*****************************************************************
void TAdvPageRef::lockGuardPageForRead(FPOINTER a_fpPage)
{
    TPageRefMapIter iter = m_pData->m_guardPageRefs.find(a_fpPage);

    if(iter == m_pData->m_guardPageRefs.end())
    {
        m_pStorage->releasePage(m_pData->m_pageRef);
    
        DataPage::TPageRef pageRef;
        m_pStorage->getPageForRead(a_fpPage, pageRef);
        m_pData->m_guardPageRefs[a_fpPage] = pageRef;
        m_setLockedPages.insert(a_fpPage);
    }
}

//*****************************************************************
void TAdvPageRef::lockGuardPageForWrite(FPOINTER a_fpPage)
{
    m_pData->beginTrans();

    TPageRefMapIter iter = m_pData->m_guardPageRefs.find(a_fpPage);

    if(iter == m_pData->m_guardPageRefs.end())
    {
        m_pStorage->releasePage(m_pData->m_pageRef);

        DataPage::TPageRef pageRef;
        m_pStorage->getPageForWrite(a_fpPage, pageRef);
        m_pData->m_guardPageRefs[a_fpPage] = pageRef;
        m_setLockedPages.insert(a_fpPage);
    }
    else
    {
        DataPage::TPageRef &pageRef = iter->second;
        if(pageRef.getAccess() != DataPage::TPageRef::eReadWrite)
        {
            m_pStorage->releasePage(m_pData->m_pageRef);
    
            m_pStorage->getPageForWrite(a_fpPage, pageRef);
        }
    }
}

//*****************************************************************
void TAdvPageRef::releaseGuardPage(FPOINTER a_fpPage)
{
    TPageRefMapIter iter = m_pData->m_guardPageRefs.find(a_fpPage);

    if(iter != m_pData->m_guardPageRefs.end())
    {
        DataPage::TPageRef &pageRef = iter->second;
        m_pStorage->releasePage(pageRef);
        m_pData->m_guardPageRefs.erase(iter);
    }
}

//*****************************************************************
void* TAdvPageRef::getPageForRead(FPOINTER a_fpPage)
{
    TPageRefMapIter iter = m_pData->m_guardPageRefs.find(a_fpPage);

    if(iter != m_pData->m_guardPageRefs.end())
    {
        return iter->second.getBuffer();
    }

    m_pStorage->getPageForRead(a_fpPage, m_pData->m_pageRef);
    m_pData->m_nStateHash++;
    return m_pData->m_pageRef.getBuffer();
}

//*****************************************************************
void* TAdvPageRef::getPageForWrite(FPOINTER a_fpPage)
{
    m_pData->beginTrans();

    TPageRefMapIter iter = m_pData->m_guardPageRefs.find(a_fpPage);

    if(iter != m_pData->m_guardPageRefs.end())
    {
        DataPage::TPageRef& ref = iter->second;
        if(ref.getAccess() != DataPage::TPageRef::eReadWrite)
            m_pStorage->getPageForWrite(a_fpPage, ref);

        return ref.getBuffer();
    }

    m_pStorage->getPageForWrite(a_fpPage, m_pData->m_pageRef);
    m_pData->m_nStateHash++;
    return m_pData->m_pageRef.getBuffer();
}


//*****************************************************************
void TAdvPageRef::releasePage()
{
    m_pStorage->releasePage(m_pData->m_pageRef);
}

//*****************************************************************
void TAdvPageRef::releaseAll(bool a_bEndTrans)
{
    m_pData->releaseAllPages();
    if(a_bEndTrans)
        m_pData->endTrans();
}

//*****************************************************************
FILEHEADER TAdvPageRef::getHeader()
{
    return m_pStorage->getHeader();
}

//**********************************************************
void TAdvPageRef::setHeader(FILEHEADER *a_pHeader)
{
    m_pStorage->setHeader(a_pHeader);
}

//**********************************************************
int& TAdvPageRef::getStateHashRef()
{
    return m_pData->m_nStateHash;
}

//**********************************************************
bool TPageIterator::setFirstPage(FPOINTER a_fpPage)
{
    m_fpPage = a_fpPage;
    return TStoragePageAlloc::GetTableAndPageIndex(a_fpPage, m_fpTable, m_nPageIndex, m_pageRef) ? true : false;
}

//**********************************************************
bool TPageIterator::setNextPage()
{
    PAGE_ENTRY *pEntry = (PAGE_ENTRY *)m_pageRef.getPageForRead(m_fpTable);
    if(pEntry == NULL)
    {
        m_bError = true;
        return false;
    }

    pEntry = &pEntry[m_nPageIndex];

    if(pEntry->fpNext == 0)
        return false;

    return setFirstPage(pEntry->fpNext);
}


//**********************************************************
BOOL TStoragePageAlloc::GetTableAndPageIndex(FPOINTER a_fpPage, FPOINTER &a_fpTable, int &a_nPageIndex, TAdvPageRef &a_xPageRef)
{
    int nTabIndex  = a_fpPage / PAGES_PER_TABLE;
    int nPageIndex = a_fpPage % PAGES_PER_TABLE;

    FPOINTER fpTable = GetTableDirEntry(nTabIndex, a_xPageRef);  
    if(fpTable == NULL)
        return FALSE;

    int nSubtable = nPageIndex / PAGES_PER_SUBTABLE;
    nPageIndex    = nPageIndex % PAGES_PER_SUBTABLE;

    a_fpTable    = fpTable + nSubtable;
    a_nPageIndex = nPageIndex;

    return TRUE;
}

//**********************************************************
FPOINTER TStoragePageAlloc::GetTableDirEntry(int a_nIndex, TAdvPageRef &a_xPageRef)
{
    int nDirIndex = a_nIndex / TABLES_PER_DIR;
    int nTabIndex = a_nIndex % TABLES_PER_DIR;

    FPOINTER *fpDir = (FPOINTER*)a_xPageRef.getPageForRead((FPOINTER)nDirIndex + PAGES_OF_HEADER);
    if(fpDir == NULL)
        return 0;

    return fpDir[nTabIndex];
}

//**********************************************************
BOOL TStoragePageAlloc::SetTableDirEntry(int a_nIndex, FPOINTER a_fpTable, TAdvPageRef &a_xPageRef)
{
    int nDirIndex = a_nIndex / TABLES_PER_DIR;
    int nTabIndex = a_nIndex % TABLES_PER_DIR;

    FPOINTER *fpDir = (FPOINTER*)a_xPageRef.getPageForWrite((FPOINTER)nDirIndex + PAGES_OF_HEADER);
    if(fpDir == NULL)
        return FALSE;

    return fpDir[nTabIndex] = a_fpTable;
}

//**********************************************************
BOOL TStoragePageAlloc::GetLastPageTable(int *a_nDirIndex, FPOINTER *a_fpTable, TAdvPageRef &a_xPageRef)
{
    FILEHEADER header = a_xPageRef.getHeader();

    int idxLower = 0;
    int idxUpper = header.m_nPageTabDirCount * TABLES_PER_DIR - 1; 

    FPOINTER fpLowerDirEntry = GetTableDirEntry(idxLower, a_xPageRef);
    FPOINTER fpUpperDirEntry = GetTableDirEntry(idxUpper, a_xPageRef);

    if(fpLowerDirEntry == 0)
        return 0;

    if(fpUpperDirEntry != 0)
        return fpUpperDirEntry;

    //--------------------------------------------------------------------------
    while(true)
    {
        if((idxUpper - idxLower) <= 1)
        {
            *a_nDirIndex = idxLower;
            *a_fpTable = fpLowerDirEntry;
            return TRUE;
        }
        int idxMiddle = (idxUpper + idxLower) / 2;
        FPOINTER fpMiddleDirEntry = GetTableDirEntry(idxMiddle, a_xPageRef);

        if(fpMiddleDirEntry == 0)
        {
            idxUpper = idxMiddle;
            fpUpperDirEntry = fpMiddleDirEntry;
        }
        else
        {
            idxLower = idxMiddle;
            fpLowerDirEntry = fpMiddleDirEntry;
        }
    }
    //--------------------------------------------------------------------------
    return FALSE;
}

//**********************************************************
BOOL TStoragePageAlloc::GetNextPageCount(FPOINTER a_fpFirst, int &a_nPageCount)
{
    TAdvPageRef    xPageRef;
    TPageIterator  xPageIter(xPageRef);
    TUseSpinLock   xSL(&G_slPageMgr);

    xPageRef.lockGuardPageForRead((FPOINTER)1);

    a_nPageCount = 0;
    bool bLoop = xPageIter.setFirstPage(a_fpFirst);

    while(bLoop)
    {
        a_nPageCount++;
        bLoop = xPageIter.setNextPage();
    }
    return !xPageIter.m_bError;
}

//**********************************************************
BOOL TStoragePageAlloc::GetNextPages(int a_idxBase, FPOINTER a_fpBase, int a_idxStart, 
                                     CArray<FPOINTER,FPOINTER> &a_arrPages)
{
    TAdvPageRef    xPageRef;
    TPageIterator  xPageIter(xPageRef);
    TUseSpinLock   xSL(&G_slPageMgr);
    int           &idxCurrent = a_idxBase;
    int            idxUpperBound = a_idxStart + GET_PAGE_COUNT;

    xPageRef.lockGuardPageForRead((FPOINTER)1);

    a_arrPages.RemoveAll();
    bool bLoop = xPageIter.setFirstPage(a_fpBase);

    while(bLoop)
    {
        if(idxCurrent >= idxUpperBound)
        {
            break;
        }
        else if(idxCurrent >= a_idxStart)
        {
            a_arrPages.Add(xPageIter.m_fpPage);
        }

        idxCurrent++;

        bLoop = xPageIter.setNextPage();
    }
    return !xPageIter.m_bError;
}


//**********************************************************
FPOINTER TStoragePageAlloc::AddNewPage(FPOINTER a_fpFirst, DWORD a_dwSegmentId, BOOL a_bReserve)
{
    TAdvPageRef   xPageRef;
    TPageIterator xPageIter(xPageRef);
    TUseSpinLock  xSL(&G_slPageMgr);

    xPageRef.lockGuardPageForWrite((FPOINTER)1);

    bool bLoop = xPageIter.setFirstPage(a_fpFirst);
    while(bLoop)
    {
        bLoop = xPageIter.setNextPage();
    }

    if(!xPageIter.m_bError)
    {
        FPOINTER fpNewPage = CreateNewPage(a_dwSegmentId, a_bReserve);
        if (fpNewPage > 0)
        {
            PAGE_ENTRY* pPageTable = (PAGE_ENTRY *)xPageRef.getPageForWrite(xPageIter.m_fpTable);
            if (pPageTable == NULL)
               return 0; 
            pPageTable[xPageIter.m_nPageIndex].fpNext    = fpNewPage;
            pPageTable[xPageIter.m_nPageIndex].dwSegmentId = a_dwSegmentId;
            return fpNewPage;
        }
    }
    return 0;
}

//**********************************************************
FPOINTER TStoragePageAlloc::CreateNewPage(DWORD a_dwSegmentId, BOOL a_bReserve)
{
    TAdvPageRef    xPageRef;
    FPOINTER       fpTable    = 0;
    FPOINTER       fpPage     = 0;
    PAGE_ENTRY    *pPageSubtable = NULL;
    LPVOID         pvPage     = NULL; 
    TUseSpinLock   xSL(&G_slPageMgr);
    int            nTabIndex = 0;

    xPageRef.lockGuardPageForWrite((FPOINTER)1);

    GetLastPageTable(&nTabIndex, &fpTable, xPageRef);

    while(true)
    {
        if (fpTable==0)
        {
            if (!CreateNewTable(nTabIndex))
                return 0;
            fpTable = GetTableDirEntry(nTabIndex, xPageRef);
        }
        for(int nSubtable = 0; nSubtable < SUBTABLE_COUNT; nSubtable++)
        {
            FPOINTER fpSubtable = fpTable + nSubtable;

            pPageSubtable = (PAGE_ENTRY *)xPageRef.getPageForRead(fpSubtable);
            if (pPageSubtable != NULL)
            {
                for (int nPage = 0; nPage < PAGES_PER_SUBTABLE; nPage++)
                {
                    if (pPageSubtable[nPage].dwSegmentId==0)
                    {
                        pPageSubtable = (PAGE_ENTRY *)xPageRef.getPageForWrite(fpSubtable);
                        if(pPageSubtable == NULL)
                            return 0;
                        fpPage 
                            = PAGES_PER_TABLE * nTabIndex 
                            + PAGES_PER_SUBTABLE * nSubtable
                            + nPage;
                        pPageSubtable[nPage].dwSegmentId = a_dwSegmentId;
                        pPageSubtable[nPage].fpNext    = 0;
                    
                        // Zeruj stronê
                        pvPage = xPageRef.getPageForWrite(fpPage);
                        if (pvPage)
                            memset(pvPage, 0, PAGESIZE);
                        return fpPage;
                    }
                }
            }
        }
        if (pPageSubtable==NULL)
            break;

        fpTable = GetTableDirEntry(++nTabIndex, xPageRef);
    }
    return 0;
}

//**********************************************************
BOOL TStoragePageAlloc::FreeDataChain(FPOINTER a_fpFirst, BOOL a_bTruncate)
{
    CArray<FPOINTER, FPOINTER> arrFP;
    TAdvPageRef       xPageRef;
    TPageIterator     xPageIter(xPageRef);
    BOOL              bInclude = !a_bTruncate; 
    TUseSpinLock      xSL(&G_slPageMgr);
    FILEHEADER        header = xPageRef.getHeader();

    xPageRef.lockGuardPageForWrite((FPOINTER)1);

    bool bLoop = xPageIter.setFirstPage(a_fpFirst);
    while(bLoop)
    {
        FPOINTER fpPage  = xPageIter.m_fpPage;
        FPOINTER fpTable = xPageIter.m_fpTable;
        int      nPageIndex = xPageIter.m_nPageIndex;

        bLoop = xPageIter.setNextPage();
        if(xPageIter.m_bError)
            return FALSE;

        PAGE_ENTRY *pPageTable = (PAGE_ENTRY *)xPageRef.getPageForWrite(fpTable);
        if (pPageTable != NULL)
        {
            pPageTable[nPageIndex].fpNext = 0;
            if (bInclude)
            {
                pPageTable[nPageIndex].dwSegmentId = 0;
                arrFP.Add(fpPage);
            }
            bInclude = TRUE;
        }
        if (pPageTable == NULL)
            return FALSE;
    }
    
//    FILE_Lock();
    {
        long nFree = -1;
        do{
            nFree = -1;
            int nSize = arrFP.GetSize();
            int ctr = 0;

            for (ctr = 0; ctr < nSize; ctr++)
            {
                FPOINTER fpPage = arrFP[ctr] + 1;
                if (fpPage == header.m_fpEnd)
                {
                    nFree = ctr;
                    header.m_bDirty = TRUE;
                    xPageRef.setHeader(&header);
                    TStorage *pStorage = xPageRef.getStorage();
                    pStorage->setTopPointer(header.m_fpEnd - 1);
                }
                else
                {
                    TStorage *pStorage = xPageRef.getStorage();
                    pStorage->setFlags(eFlag_NeedCompact, eFlag_NeedCompact);
                }
            }
            if (nFree!=-1)
                arrFP.RemoveAt(nFree);
        }while(nFree!=-1);
    }
//    FILE_Unlock();

    return TRUE;
}

//**********************************************************
BOOL TStoragePageAlloc::CreateNewTable(long a_nIndex)
{
    TAdvPageRef     xPageRef;
    FPOINTER        fpNewTable = 0;
    FILEHEADER      header    = xPageRef.getHeader();
    PAGE_ENTRY     *pPageTable = NULL;
    long            nIndex  = 0;
    long            ctr     = 0;
    BOOL            bRetVal = FALSE;

    if (a_nIndex == 0)
    {
        fpNewTable = PAGES_OF_HEADER + header.m_nPageTabDirCount; //not PAGESIZE_CRC
        nIndex     = fpNewTable;
    }
    else
    {
        fpNewTable = a_nIndex * PAGES_PER_TABLE;
        nIndex = 0;
    }
    SetTableDirEntry(a_nIndex, fpNewTable, xPageRef);

    for(int nSubtable = 0; nSubtable < SUBTABLE_COUNT; nSubtable++)
    {
        pPageTable = (PAGE_ENTRY*)xPageRef.getPageForWrite(fpNewTable);
        if (pPageTable!=NULL)
        {
            memset(pPageTable, 0, PAGESIZE);

            if(nSubtable == 0)
            {
                for (ctr = 0; ctr < nIndex; ctr++)  // for header
                {
                    pPageTable[ctr].dwSegmentId = SEGID_RESERVED;
                    pPageTable[ctr].fpNext    = DWORD(-1);
                }
                for(ctr = 0; ctr < SUBTABLE_COUNT; ctr++)
                {
                    pPageTable[nIndex + ctr].dwSegmentId = SEGID_TABLE;
                    pPageTable[nIndex + ctr].fpNext    = 0;
                }
            }
            bRetVal = TRUE;
        }
        fpNewTable++;
    }
    return bRetVal;
}

