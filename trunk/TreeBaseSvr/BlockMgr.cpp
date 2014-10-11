#include "stdafx.h"
#include "BlockMgr.h"
#include "DataMgr.h"
#include "TBaseFile.h"
#include "TTask.h"
#include "TPageCache.h"
#include "TAbstractThreadLocal.h"
#include "TBlockPageIndexMap.h"
#include "TStoragePageAlloc.h"
#include <map>
#include "TreeBase.h"

using namespace Storage;

TThreadLocal<TBlockPageIndexMap> G_blkPageIndexMap;

//{{tmp
#define FILE_FileFromId(x) m_pFile
//}}tmp
static void FreeUnusedBlockDirPages(TBlock *A_pBlock);

//**********************************************************
void TBlock::InitSystemBlock(long A_ndbId, long A_nId)
{
    m_ndbId = A_ndbId;
    m_nId   = A_nId;

    Storage::TStorage *pStorage = G_dbManager.GetFile(A_ndbId);
    FILEHEADER hdr = pStorage->getHeader();
    switch(A_nId)
    {
        case BLKID_BLOCKDIR:
            m_fpFirst = hdr.m_fpBlockDirPage;
            break;
        case BLKID_SECDATA:
            m_fpFirst = hdr.m_fpSecDataPage;
            break;
        default:
            assert(false);
    }
}

//**********************************************************
static BOOL Init()
{
    return TRUE;
} 

IMPLEMENT_INIT(Init)


//**********************************************************
TTrans::TTrans(long A_nFileID)
{
    m_pStorage = G_dbManager.GetFile(A_nFileID);
    if (m_pStorage)
        m_pStorage->beginTrans();
}

//**********************************************************
TTrans::TTrans(TStorage *a_pStorage): m_pStorage(a_pStorage)
{
    if (m_pStorage)
        m_pStorage->beginTrans();
}

//**********************************************************
TTrans::~TTrans()
{
    if (m_pStorage)
        m_pStorage->endTrans();
}

//*************************************************************************
void FILE_Flush()
{
    TStorage *pStorage = NULL;
    long               ctr    = 0;
    long               nCount = G_dbManager.GetCount();
    //----------------------------------------
    for(ctr=0; ctr<nCount; ctr++)
    {
        pStorage = G_dbManager.GetFile(ctr);
        if(pStorage)
            pStorage->flush();
    }
    //----------------------------------------
}

//**********************************************************
BOOL FILE_InitStorage(long A_ndbID, TStorage *A_pStorage)
{
    FPOINTER    fpBlockDir = 0;

    if (!A_pStorage)
        return FALSE;

    TASK_FixStorage(A_pStorage);
    TMultiPageRef    xPageRef;

    FILEHEADER header = xPageRef.getHeader();

    if (header.m_fpBlockDirPage == 0)
    {
        header.m_fpBlockDirPage = TStoragePageAlloc::CreateNewPage(BLKID_BLOCKDIR, FALSE);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }
    if (header.m_fpSecDataPage == 0)
    {
        header.m_fpSecDataPage = TStoragePageAlloc::CreateNewPage(BLKID_SECDATA, FALSE);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }

    if (header.m_RootSectionBlkId == 0)
    {   
        header.m_RootSectionBlkId = TSectionBlock::CreateBlock(A_ndbID, 0, TBATTR_NOVALUES, NULL);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }
    if (header.m_LabelSectionBlkId == 0)
    {
        header.m_LabelSectionBlkId = TSectionBlock::CreateBlock(A_ndbID, 0, TBATTR_NOVALUES, NULL);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }

    return TRUE;
};

//**********************************************************
BOOL TBlock::M_FixStorage()
{
    TDBFileEntry  *pEntry   = G_dbManager[m_ndbId];
    TStorage      *pStorage = pEntry ? pEntry->m_pStorage : NULL;
    DWORD          dwTime = GetTickCount(); 
    DWORD          dwTimeDiff = 0; 
    CFileStatus    status;

    TASK_FixStorage(pStorage);

    if(pEntry->m_bCheckMedia)
    {
        dwTimeDiff = dwTime - pEntry->m_dwLastAccessTime;
        if(dwTimeDiff > 2000)
        {
            if(!CFile::GetStatus(pEntry->m_strFile, status))
                return FALSE;
        }
        pEntry->m_dwLastAccessTime = dwTime;
    }
    return TRUE;
}

//**********************************************************
FPOINTER TBlock::GetPageByIndex(int A_nIndex)
{
    if(A_nIndex == 0)
        return m_fpFirst;

    if(A_nIndex >= GetPageCount())
        return 0;

    TBlockPageIndexMap& blkMapCache = G_blkPageIndexMap.Value();

    FPOINTER fpPage = blkMapCache.find(m_ndbId, m_nId, A_nIndex);
    if(fpPage == 0)
    {
        std::pair<int, FPOINTER> base = blkMapCache.getLowerIndex(m_ndbId, m_nId, A_nIndex);
        if(base.first == 0)
            base.second = m_fpFirst;

        int nStart = A_nIndex & ~7;
        if(nStart < base.first)
            nStart = base.first;

        CArray<FPOINTER, FPOINTER> arrPages;

        if(!TStoragePageAlloc::GetNextPages(base.first, base.second, nStart, arrPages))
            return 0;

        for(int ctr = 0; ctr < arrPages.GetCount(); ctr++)
        {
            blkMapCache(m_ndbId, m_nId, nStart + ctr) = arrPages[ctr];
        }

        fpPage = blkMapCache.find(m_ndbId, m_nId, A_nIndex);
    }
    return fpPage;
}

//**********************************************************
void* TBlock::GetPageForRead(long A_nIndex, TMultiPageRef &A_PageRef)
{
    if(!M_FixStorage())
        return NULL;

    FPOINTER fpPage = GetPageByIndex(A_nIndex);
    if(fpPage == 0)
        return NULL;

    return A_PageRef.getPageForRead(fpPage);
};

//**********************************************************
void* TBlock::GetPageForWrite(long A_nIndex, TMultiPageRef &A_PageRef)
{
    if(!M_FixStorage())
        return NULL;

    FPOINTER fpPage = GetPageByIndex(A_nIndex);
    if(fpPage == 0)
        return NULL;

    return A_PageRef.getPageForWrite(fpPage);
};


//**********************************************************
long TBlock::AddNewPage(BOOL A_bReserve)
{
    long nCount = GetPageCount();

    if (nCount < 1)
        return -1;

    TBlockPageIndexMap& blkMapCache = G_blkPageIndexMap.Value();

    M_FixStorage();
    std::pair<int, FPOINTER> lower = blkMapCache.getLowerIndex(m_ndbId, m_nId, nCount);

    if(lower.first == 0)
        lower.second = m_fpFirst;

    FPOINTER fpNew = TStoragePageAlloc::AddNewPage(lower.second, m_nId, A_bReserve);
    if (fpNew == 0)
        return -1;

    m_nPageCount++;
    return nCount;
}

//**********************************************************
BOOL TBlock::Truncate(long A_nPage)
{
    FPOINTER fpPage = GetPageByIndex(A_nPage-1);
    if(fpPage == 0)
        return FALSE;

    m_nPageCount = -1; // reset
    TStoragePageAlloc::FreeDataChain(fpPage, TRUE);
    G_blkPageIndexMap.Value().removeBlockPages(m_ndbId, m_nId);
    return TRUE;
}

//**********************************************************
static BOOL IsTokenCompatible(BLOCK_ENTRY *A_pEntry)
{
    TTask *pTask  = TASK_GetCurrent();

    if (pTask==NULL)
        return TRUE;

    if (!A_pEntry->token.m_bUseToken)
        return TRUE;

    return (memcmp(&A_pEntry->token, &pTask->m_token, sizeof(ACCESS_TOKEN))==0);
}

//**********************************************************
static BOOL FillBlockEntry(BLOCK_ENTRY *A_pEntry, long A_nBlockId, DWORD A_dwMask)
{
    FPOINTER  fpPage = 0;
    TTask    *pTask  = TASK_GetCurrent(); 

    A_pEntry->dwBlockID = A_nBlockId;
    A_pEntry->dwBlockMask = A_dwMask;
    A_pEntry->wRefCount   = 1;
    fpPage = TStoragePageAlloc::CreateNewPage(A_nBlockId, TRUE);
    if (fpPage)
    {
        A_pEntry->dwFirstPage = fpPage;
        if (pTask && (A_dwMask != TBACCESS_FULL))
            A_pEntry->token = pTask->m_token;
        return TRUE;
    }
    else
    {
        A_pEntry->dwBlockID = 0;
    }
    return FALSE;
}

//**********************************************************
long BLOCK_Create(long A_ndbId, DWORD A_dwMask)
{
    TMultiPageRef  xPageRef(A_ndbId);
    long           nIdx           = 0;
    long           ctr            = 0;
    long           ctr2           = 0;
    long           nCount         = 0;
    long           nBlkId         = 0; 
    BLOCK_ENTRY   *pEntry         = NULL;
    BOOL           bExit          = FALSE;
    TDBFileEntry  *pFileEntry     = G_dbManager[A_ndbId];

    if(pFileEntry == NULL)
        return FALSE;

    xPageRef.lockGuardPageForWrite((FPOINTER)1);
    TSystemBlock<TBlock, BLKID_BLOCKDIR> blockDir(A_ndbId);

    nCount = blockDir.GetPageCount();
    for(ctr=0; ctr<nCount && !bExit; ctr++)
    {
        pEntry = (BLOCK_ENTRY *)blockDir.GetPageForWrite(ctr, xPageRef);
        if(pEntry)
            for (ctr2=0; ctr2<BLOCKS_PER_PAGE && !bExit; ctr2++)
            {
                if (pEntry[ctr2].dwBlockID==0)
                {
                    nBlkId = ctr * BLOCKS_PER_PAGE + ctr2;
                    nBlkId++;
                    if (FillBlockEntry(&pEntry[ctr2], nBlkId, A_dwMask))
                    {
                        bExit = true;
                    }
                }
            }
    }
    if (!bExit)
    {
        nIdx = blockDir.AddNewPage();
        if (nIdx!=-1)
        {
            nBlkId = 0;
            pEntry = (BLOCK_ENTRY *)blockDir.GetPageForWrite(nIdx, xPageRef);
            if(pEntry)
            {
                nBlkId = nIdx * BLOCKS_PER_PAGE;
                nBlkId++;
                FillBlockEntry(pEntry, nBlkId, A_dwMask);
            }
        }
        else
        {
            nBlkId = 0;
        }
    }
    return nBlkId;
}

//**********************************************************
BOOL BLOCK_Assign(long A_ndbId, long A_nBlockId, TBlock *A_pBlock, BOOL &A_bReadOnly)
{
    TMultiPageRef  xPageRef(A_ndbId);
    long           nPage  = (A_nBlockId-1) / BLOCKS_PER_PAGE;
    long           nIndex = (A_nBlockId-1) % BLOCKS_PER_PAGE;
    BLOCK_ENTRY   *pEntry = NULL;
    FPOINTER       fpPage = 0;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE, bTokenCompatible=FALSE, bOK=FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[A_ndbId];
    TStorage      *pStorage      = pFileEntry ? pFileEntry->m_pStorage : NULL; 
    BOOL           bFileReadOnly = pStorage ? pStorage->isReadOnly() : TRUE;

    if(pFileEntry == NULL)
        return FALSE;

    xPageRef.lockGuardPageForRead((FPOINTER)1);
    TSystemBlock<TBlock, BLKID_BLOCKDIR> blockDir(A_ndbId);

    A_bReadOnly = FALSE;

    pEntry = (BLOCK_ENTRY *)blockDir.GetPageForRead(nPage, xPageRef);
    if(pEntry)
    {
        pEntry = &pEntry[nIndex];
        if (pEntry->dwBlockID==DWORD(A_nBlockId))
        {
            bTokenCompatible = IsTokenCompatible(pEntry);
            bAccept = TRUE;
            if ((pEntry->dwBlockMask & TBACCESS_ALLOWREAD)==0)
            {
                bAccept = bTokenCompatible;
            }
            if (bAccept)
            {
                if(bFileReadOnly)
                {
                    A_bReadOnly = TRUE;
                }
                else if (!bTokenCompatible)
                {
                    if ((pEntry->dwBlockMask & TBACCESS_ALLOWWRITE)!=TBACCESS_ALLOWWRITE)
                        A_bReadOnly = TRUE;
                }
                if (!A_pBlock->m_bAssigned)
                {
                    fpPage = pEntry->dwFirstPage;
                    A_pBlock->m_fpFirst = fpPage;
                    A_pBlock->m_dwMask  = pEntry->dwBlockMask;
                    A_pBlock->m_nId     = A_nBlockId;
                    A_pBlock->m_ndbId   = A_ndbId;
                    A_pBlock->m_bAssigned = TRUE;
                }
                bRetVal = TRUE;
            }
            else
            {
                TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL);
            }
        }
    }
    return bRetVal;
}

//**********************************************************
BOOL BLOCK_AddRef(long A_ndbId, long A_nBlockId)
{
    TMultiPageRef  xPageRef(A_ndbId);
    long           nPage  = (A_nBlockId-1) / BLOCKS_PER_PAGE;
    long           nIndex = (A_nBlockId-1) % BLOCKS_PER_PAGE;
    BLOCK_ENTRY   *pEntry = NULL;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[A_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForWrite((FPOINTER)1);
        TSystemBlock<TBlock, BLKID_BLOCKDIR> blockDir(A_ndbId);

        pEntry = (BLOCK_ENTRY *)blockDir.GetPageForWrite(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwBlockID==DWORD(A_nBlockId))
            {
                bAccept = TRUE;
                if ((pEntry->dwBlockMask & TBACCESS_ALLOWLINK)==0)
                {
                    bAccept = IsTokenCompatible(pEntry);
                }
                if (bAccept)
                {
                    pEntry->wRefCount++;
                    bRetVal = TRUE;
                }
                else
                {
                    TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL);
                }
            }
        }
    };
    return bRetVal;
}

//**********************************************************
long BLOCK_GetRefCount(long A_ndbId, long A_nBlockId)
{
    TMultiPageRef   xPageRef(A_ndbId);
    long            nPage  = (A_nBlockId-1) / BLOCKS_PER_PAGE;
    long            nIndex = (A_nBlockId-1) % BLOCKS_PER_PAGE;
    long            nRefCount = -1;   
    BLOCK_ENTRY    *pEntry = NULL;
    TDBFileEntry   *pFileEntry = G_dbManager[A_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForRead((FPOINTER)1);
        TSystemBlock<TBlock, BLKID_BLOCKDIR> blockDir(A_ndbId);

        pEntry = (BLOCK_ENTRY *)blockDir.GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwBlockID==DWORD(A_nBlockId))
            {
                nRefCount = pEntry->wRefCount;
            }

            return nRefCount;
        }
    }
    return -1;
}


//**********************************************************
BOOL BLOCK_Delete(long A_ndbId, long A_nBlockId)
{
    TMultiPageRef  xPageRef(A_ndbId);
    long           nPage  = (A_nBlockId-1) / BLOCKS_PER_PAGE;
    long           nIndex = (A_nBlockId-1) % BLOCKS_PER_PAGE;
    BLOCK_ENTRY   *pEntry = NULL;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE;
    BOOL           bDeleted = FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[A_ndbId];

    if(pFileEntry)
    {
        G_blkPageIndexMap.Value().removeBlockPages(A_ndbId, A_nBlockId);

        xPageRef.lockGuardPageForWrite((FPOINTER)1);
        TSystemBlock<TBlock, BLKID_BLOCKDIR> blockDir(A_ndbId);

        pEntry = (BLOCK_ENTRY *)blockDir.GetPageForWrite(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwBlockID==DWORD(A_nBlockId))
            {
                bAccept = TRUE;
                if ((pEntry->dwBlockMask & TBACCESS_ALLOWDELETE)!=TBACCESS_ALLOWDELETE)
                {
                    bAccept = IsTokenCompatible(pEntry);
                }
                if (bAccept)
                {
                    if ((--pEntry->wRefCount)==0)
                    {
                        TStoragePageAlloc::FreeDataChain(pEntry->dwFirstPage, FALSE);
                        memset(pEntry, 0, sizeof(BLOCK_ENTRY));
                        bDeleted = TRUE;
                    };
                    bRetVal = TRUE;
                }
                else
                {
                    TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL);
                }
            }
        }
        if (bDeleted)
        {
            FreeUnusedBlockDirPages(&blockDir);
        }
    }
    return bRetVal;
}

//**********************************************************
long BLOCK_GetRootSection(long A_ndbId)
{
    TStorage   *pStorage = G_dbManager.GetFile(A_ndbId);
    FILEHEADER  header   = pStorage->getHeader();

    return header.m_RootSectionBlkId;
}

//**********************************************************
long BLOCK_GetLabelSection(long A_ndbId)
{
    TStorage    *pStorage = G_dbManager.GetFile(A_ndbId);
    FILEHEADER   header   = pStorage->getHeader();

    return header.m_LabelSectionBlkId;
}

//**********************************************************
static void FreeUnusedBlockDirPages(TBlock *A_pBlock)
{
    TMultiPageRef  xPageRef(A_pBlock->m_ndbId);
    long           nPage = 0, nPageCount = 0, ctr = 0;
    BLOCK_ENTRY   *pEntry = NULL;
    long           nMaxUsed = 0;
    
    if (!A_pBlock)
        return;

    nPageCount = A_pBlock->GetPageCount();
    for (nPage=0; nPage<nPageCount; nPage++)
    {
        pEntry = (BLOCK_ENTRY *)A_pBlock->GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            for (ctr=0; ctr<BLOCKS_PER_PAGE; ctr++)
            {
                if (pEntry[ctr].dwBlockID!=0)
                {
                    nMaxUsed = nPage;
                    break;
                }
            }
        }
    }
    if (nMaxUsed<(nPageCount-1))
        A_pBlock->Resize(nMaxUsed+1);
}

//**********************************************************
int TBlock::GetPageCount()
{
    if(m_nPageCount == -1)
    {
        TStoragePageAlloc::GetNextPageCount(m_fpFirst, m_nPageCount);
    }
    return m_nPageCount;
}


//**********************************************************
BOOL TBlock::Resize(long A_nPageCount)
{
    assert(A_nPageCount > 0);

    long nCount = GetPageCount();
    long ctr = 0;
    BOOL bRetVal = FALSE;
    long nIndex  = 0;
    FPOINTER fpPage = 0;
    FPOINTER fpMax  = 0;

    if (nCount==A_nPageCount)
        return TRUE;

    M_FixStorage();
    if (nCount>A_nPageCount)
    {
        Truncate(A_nPageCount);
        return TRUE;
    }
    else
    {
        bRetVal = TRUE;
        nCount = A_nPageCount-nCount;
        //----------------------------------------
        for (ctr=0; ctr<nCount; ctr++)
        {
            nIndex = AddNewPage();
            if (nIndex==-1)
            {
                bRetVal = FALSE;
                break;
            };
            fpPage = GetPageByIndex(nIndex);
            if (fpMax<fpPage)
                fpMax = fpPage;
        }
        //----------------------------------------
        if (!bRetVal)
        {
            Truncate(nCount);
        }
    }
    return bRetVal;
}


//*********************************************************
long TSectionBlock::CreateBlock(int a_ndbId, int A_blkIdParent, DWORD A_dwAttrs, const SECURITY_DESCRIPTOR *A_pSecDesc)
{
    // do privileged
    long   nBlkId = BLOCK_Create(a_ndbId, /*temporarily*/TBACCESS_FULL);
    BOOL   bReadOnly = FALSE;

    Storage::TMultiPageRef xPageRef;

    TBlock blk;
    if(!BLOCK_Assign(a_ndbId, nBlkId, &blk, bReadOnly) || bReadOnly)
        return 0;

    SECTION_HEADER *pHeader = (SECTION_HEADER *)blk.GetPageForWrite(0, xPageRef);
    pHeader->m_dwAttrs = A_dwAttrs;

    return nBlkId;
}
