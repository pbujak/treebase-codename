#include "stdafx.h"
#include "compact.h"
#include "Storage.h"
#include "TStoragePageAlloc.h"
#include "TSegmentPageIndexMap.h"
#include "SegmentEntry.h"
#include "TBaseFile.h"

using namespace Storage;

extern TThreadLocal<TSegmentPageIndexMap> G_segPageIndexMap;

//**********************************************************
template<>
void Util::TCacheEntryOperations::initEntryData<SCompactPageInfo>(SCompactPageInfo *a_pEntry)
{
    a_pEntry->pvBuff = NULL;
}

//**********************************************************
template<>
void Util::TCacheEntryOperations::freeEntryData<SCompactPageInfo>(SCompactPageInfo *a_pEntry)
{
    if(a_pEntry->pvBuff)
    {
       if(a_pEntry->mode == SCompactPageInfo::eWrite && a_pEntry->pStorage)
       {
           Storage::TAdvPageRef xPageRef(a_pEntry->pStorage);
           void *pvDest = xPageRef.getPageForWrite(a_pEntry->fpPage);
           memcpy(pvDest, a_pEntry->pvBuff, PAGESIZE);
       }
       DataPage::TPagePool::getInstance().releaseBlock(a_pEntry->pvBuff);
    }
}

//**********************************************************
void* TCompactBase::GetPage(FPOINTER a_fpPage, SCompactPageInfo::EMode a_mode)
{
    SCompactPageInfo *pPageInfo = m_localPageCache.findPage(a_fpPage);

    if(pPageInfo == NULL)
    {
        pPageInfo = m_localPageCache.addNewPage(a_fpPage);

        pPageInfo->pStorage = m_pStorage;
        pPageInfo->pvBuff = DataPage::TPagePool::getInstance().getBlock();

        TAdvPageRef xPageRef(m_pStorage);
        void *pvSrc = xPageRef.getPageForRead(a_fpPage);
        memcpy(pPageInfo->pvBuff, pvSrc, PAGESIZE);

        pPageInfo->mode = a_mode;
    }
    else if(a_mode == SCompactPageInfo::eWrite)
    {
        pPageInfo->mode = a_mode;
    }

    return pPageInfo->pvBuff;
}

//**********************************************************
BOOL TCompactBase::UpdateNextPage(FPOINTER a_fpFirst, FPOINTER a_fpOldNext, FPOINTER a_fpNewNext)
{
    TAdvPageRef xRefPage(m_pStorage);

    PAGE_ENTRY *pPageTable = NULL;
    int         nPrevIndex = 0;  
    int         nPageIndex = 0;  
    FPOINTER    fpPage     = a_fpFirst;
    FPOINTER    fpTable    = 0;

    do
    {
        TStoragePageAlloc::GetTableAndPageIndex(fpPage, fpTable, nPageIndex, xRefPage);
        pPageTable = (PAGE_ENTRY *)GetPage(fpTable, SCompactPageInfo::eWrite);

        if(pPageTable)
        {
            nPrevIndex = nPageIndex;
            fpPage = pPageTable[nPageIndex].fpNext;
            if (fpPage == a_fpOldNext)
            {
                pPageTable[nPrevIndex].fpNext = a_fpNewNext;
                return TRUE;
            }
        }
    }
    while(fpPage != 0);
    return FALSE;
};

//**********************************************************
BOOL TCompactBase::UpdateSegmentDir(long a_nSegmentId, FPOINTER a_fpOldNext, FPOINTER a_fpNewNext)
{
    TAdvPageRef xRefPage(m_pStorage);
    long         nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long         nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    SEGMENT_ENTRY *pEntry = NULL;
    BOOL         bRetVal = FALSE;

    TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(m_ndbId);

    Flush();
    pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForWrite(nPage, xRefPage);
    if(pEntry)
    {
        pEntry = &pEntry[nIndex];
        if (pEntry->dwSegmentID==DWORD(a_nSegmentId))
        {
            if (pEntry->dwFirstPage==a_fpOldNext)
            {
                pEntry->dwFirstPage = a_fpNewNext;
            }
            else
                UpdateNextPage(pEntry->dwFirstPage, a_fpOldNext, a_fpNewNext);
        }
    }
    return bRetVal;
};

//**********************************************************
void TCompactBase::MoveTable(FPOINTER fpDest, FPOINTER fpSrc)
{
    TAdvPageRef xRefPage(m_pStorage);
    xRefPage.beginTrans();
        
    FILEHEADER  header = xRefPage.getHeader();

    // Update directory
    int nTabCount = TABLES_PER_DIR * header.m_nPageTabDirCount;
    for (int ctr=0; ctr < nTabCount; ctr++)
    {
        if (TStoragePageAlloc::GetTableDirEntry(ctr, xRefPage) == fpSrc)
        {
            TStoragePageAlloc::SetTableDirEntry(ctr, fpDest, xRefPage);
            break;
        }
    }

    // Move content
    for(int nSubTable = 0; nSubTable < SUBTABLE_COUNT; nSubTable++)
    {
        void* pvSrc  = GetPage(fpSrc + nSubTable,  SCompactPageInfo::eRead);
        void* pvDest = GetPage(fpDest + nSubTable, SCompactPageInfo::eWrite);
        memcpy(pvDest, pvSrc, PAGESIZE);
    }
    Flush();

    // Update tables
    for(int nSubTable = 0; nSubTable < SUBTABLE_COUNT; nSubTable++)
    {
        FPOINTER    fpTableSrc  = 0;
        FPOINTER    fpTableDest = 0;

        int nPageIndexSrc;
        int nPageIndexDest;

        TStoragePageAlloc::GetTableAndPageIndex(fpSrc + nSubTable,  fpTableSrc,  nPageIndexSrc, xRefPage);
        TStoragePageAlloc::GetTableAndPageIndex(fpDest + nSubTable, fpTableDest, nPageIndexDest, xRefPage);

        PAGE_ENTRY *pPageTableSrc  = (PAGE_ENTRY *)GetPage(fpTableSrc,  SCompactPageInfo::eWrite);
        PAGE_ENTRY *pPageTableDest = (PAGE_ENTRY *)GetPage(fpTableDest, SCompactPageInfo::eWrite);

        pPageTableSrc[nPageIndexSrc].dwSegmentId  = 0;
        pPageTableDest[nPageIndexDest].dwSegmentId = SEGID_TABLE;
    }
    Flush();
}


//**********************************************************
void TCompactBase::MovePage(FPOINTER fpDest, FPOINTER fpSrc)
{
    TAdvPageRef xRefPage(m_pStorage);

    FILEHEADER  header         = xRefPage.getHeader();
    PAGE_ENTRY *pPageTableSrc  = NULL;
    PAGE_ENTRY *pPageTableDest = NULL;
    int         nPageIndexSrc  = 0;  
    int         nPageIndexDest = 0;  
    FPOINTER    fpTableSrc     = 0;
    FPOINTER    fpTableDest    = 0;
    FPOINTER    fpTable        = 0;
    long        nSegId         = 0; 
    long        ctr            = 0; 
    LPVOID      pvSrc=NULL, pvDest=NULL;

    TStoragePageAlloc::GetTableAndPageIndex(fpSrc,  fpTableSrc,  nPageIndexSrc, xRefPage);
    TStoragePageAlloc::GetTableAndPageIndex(fpDest, fpTableDest, nPageIndexDest, xRefPage);

    pPageTableSrc  = (PAGE_ENTRY *)GetPage(fpTableSrc,  SCompactPageInfo::eWrite);
    pPageTableDest = (PAGE_ENTRY *)GetPage(fpTableDest, SCompactPageInfo::eWrite);
    if (pPageTableSrc==NULL || pPageTableDest==NULL)
        return;

    nSegId = pPageTableSrc[nPageIndexSrc].dwSegmentId;   

    pvSrc  = GetPage(fpSrc,  SCompactPageInfo::eRead);
    pvDest = GetPage(fpDest, SCompactPageInfo::eWrite);
    if (pvSrc==NULL || pvDest==NULL)
        return;

    memcpy(pvDest, pvSrc, PAGESIZE);

    assert(nSegId != SEGID_TABLE);

    // Przenieœ wpisy
    pPageTableDest[nPageIndexDest].dwSegmentId = nSegId;
    pPageTableDest[nPageIndexDest].fpNext    = pPageTableSrc[nPageIndexSrc].fpNext;
    pPageTableSrc[nPageIndexSrc].dwSegmentId   = 0;

    if (nSegId == SEGID_SEGMENTDIR)
    {
        if (header.m_fpSegmentDirPage == fpSrc)
        {
            header.m_fpSegmentDirPage = fpDest;
            xRefPage.setHeader(&header);
        }
        else
            UpdateNextPage(header.m_fpSegmentDirPage, fpSrc, fpDest);

    }
    else if (nSegId == SEGID_SECDATA)
    {
        if (header.m_fpSecDataPage == fpSrc)
        {
            header.m_fpSecDataPage = fpDest;
            xRefPage.setHeader(&header);
        }
        else
            UpdateNextPage(header.m_fpSecDataPage, fpSrc, fpDest);

    }
    else if (nSegId != SEGID_TABLE)
    {
        UpdateSegmentDir(nSegId, fpSrc,fpDest);
    }
    if (header.m_fpEnd == fpSrc && header.m_fpEnd > fpDest)
    {
        m_pStorage->setTopPointer(header.m_fpEnd - 1);
    }
};

//**********************************************************
BOOL TCompactBase::IsPageFree(FPOINTER a_fpPage, BOOL &a_bFree)
{
    DWORD dwSegmentId = GetPageSegmentId(a_fpPage);
    a_bFree = (dwSegmentId == 0);
    return TRUE;
}

//**********************************************************
void TCompactBase::Flush()
{
    m_localPageCache.reset();
}

//**********************************************************
DWORD TCompactBase::GetPageSegmentId(FPOINTER a_fpPage)
{
    TAdvPageRef  xRefPage(m_pStorage);

    int nPageIndex = a_fpPage % PAGES_PER_TABLE;
    FPOINTER fpTable = 0; 

    TStoragePageAlloc::GetTableAndPageIndex(a_fpPage, fpTable, nPageIndex, xRefPage);

    PAGE_ENTRY *pPageTable = (PAGE_ENTRY *)GetPage(fpTable, SCompactPageInfo::eRead);
    
    return pPageTable[nPageIndex].dwSegmentId;
}

//**********************************************************
BOOL TCompactBase::Compact(long a_ndbId)
{
    TAdvPageRef  xRefPage(a_ndbId);
    FILEHEADER   header = xRefPage.getHeader();
    FPOINTER     fpGapStart = header.m_nPageTabDirCount;
    FPOINTER     fpGapEnd   = fpGapStart;
    DWORD        segId, segIdPrev = -1;

    m_ndbId    = a_ndbId;
    m_pStorage = G_dbManager.GetFile(a_ndbId);

    while(fpGapStart < m_pStorage->getTopPointer())
    {
        BOOL bFree = FALSE;
        while(true)
        {
            IsPageFree(fpGapEnd, bFree);
            if(!bFree) break;
            fpGapEnd++;

            if(fpGapEnd >= header.m_fpEnd)
            {
                m_pStorage->setTopPointer(fpGapStart);
                return TRUE;
            };
        }
        segId = GetPageSegmentId(fpGapEnd);
        if(segIdPrev == -1)
            segIdPrev = segId;

        if(fpGapStart != fpGapEnd)
        {
            if(segId == SEGID_TABLE)
            {
                MoveTable(fpGapStart, fpGapEnd);
                fpGapStart += SUBTABLE_COUNT - 1;
                fpGapEnd   += SUBTABLE_COUNT - 1;
            }
            else
                MovePage(fpGapStart, fpGapEnd);

            G_segPageIndexMap.Value().reset();
        }
        fpGapStart++;
        fpGapEnd++;

        if(fpGapEnd == m_pStorage->getTopPointer())
        {
            m_pStorage->setTopPointer(fpGapStart);
            break;
        }

        if(segId != segIdPrev && IsOverTime())
        {
            Flush();
            xRefPage.releaseAll();
            YieldControl();

            for(; fpGapStart < fpGapEnd; fpGapStart++)
            {
                IsPageFree(fpGapStart, bFree);
                if(bFree) break;
            }
        }
        segIdPrev = segId;
    }
    return TRUE;
}

//**********************************************************
/*
BOOL TCompactBase::Compact()
{
    long        nTab        = 0;
    long        nPage       = 0; 
    long        nIndex      = 0; 
    FPOINTER    fpTable     = 0;
    FPOINTER    fpTablePrev = 0;
    FPOINTER    fpEnd       = 0;
    FPOINTER    fpLast      = 0;
    FPOINTER    fpFree      = 0;
    PAGE_ENTRY *pPageTable  = NULL;
    MSG         msg         = {0};
    BOOL        bFree       = FALSE;
    FILEHEADER  header      = xRefPage.getHeader();

    if (TTask::S_dwCount>1)
        return FALSE;

    fpEnd = header.m_fpEnd;
    //----------------------------------------------------
    do
    {
        fpTable = TStoragePageAlloc::GetTableDirEntry(nTab, xRefPage); 

        if (fpTable != 0)
        {
            if (fpTable != fpTablePrev)
                pPageTable = (PAGE_ENTRY *)xRefPage.getPageForRead(fpTable);

            if (pPageTable)
            {
                if (pPageTable[nPage].dwSegmentId == 0)
                {
                    fpFree = nIndex;
                    if (fpFree >= fpEnd - 1)
                    {
                        FILE_Flush();
                        return TRUE;
                    }

                    fpLast = fpEnd;
                    do
                    {
                        fpLast -= 1;
                        IsPageFree(fpLast, bFree);
                    }while (bFree);
                    MovePage(fpFree, fpLast);
                    fpEnd = fpLast;
                    header.m_fpEnd = fpLast;
                    header.m_bDirty = TRUE;
                    xRefPage.setHeader(&header);
                    fpTablePrev = 0;

                    if ((nIndex % 5) == 4)
                    {
                        if (PeekMessage(&msg, NULL, WM_TB_THREAD_ATTACH, WM_TB_THREAD_ATTACH, PM_NOREMOVE))
                        {
                            FILE_Flush();
                            return TRUE;
                        }
                        if (PeekMessage(&msg, NULL, WM_ENDSESSION, WM_ENDSESSION, PM_NOREMOVE))
                        {
                            FILE_Flush();
                            return TRUE;
                        }
                    }
                }
            }
        }
        nIndex++;
        nTab  = nIndex / PAGES_PER_TABLE;
        nPage = nIndex % PAGES_PER_TABLE;
        fpTablePrev = fpTable;
        Sleep(0);
        header = xRefPage.getHeader();
    }while (nTab < int(TABLES_PER_DIR * header.m_nPageTabDirCount));
    //----------------------------------------------------
    FILE_Flush();

    return TRUE;
}
*/
