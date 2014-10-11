#include "stdafx.h"
#include "SegmentMgr.h"
#include "DataMgr.h"
#include "TBaseFile.h"
#include "TTask.h"
#include "TPageCache.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TSegmentPageIndexMap.h"
#include "TStoragePageAlloc.h"
#include <map>
#include "TreeBase.h"
#include "TSecurityAttributes.h"
#include "SecurityManager.h"
#include "SegmentEntry.h"
#include "TSegmentSecurityCache.h"
#include "junction.h"

using namespace Storage;

TThreadLocal<TSegmentPageIndexMap> G_segPageIndexMap;

//{{tmp
#define FILE_FileFromId(x) m_pFile
//}}tmp
static void FreeUnusedSegmentDirPages(TSegment *a_pSegment);

//**********************************************************
TSegment::~TSegment()
{
}

//**********************************************************
void TSegment::InitSystemSegment(long a_ndbId, long a_nId)
{
    m_ndbId = a_ndbId;
    m_nId   = a_nId;

    Storage::TStorage *pStorage = G_dbManager.GetFile(a_ndbId);
    FILEHEADER hdr = pStorage->getHeader();
    switch(a_nId)
    {
        case SEGID_SEGMENTDIR:
            m_fpFirst = hdr.m_fpSegmentDirPage;
            break;
        case SEGID_SECDATA:
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
TTrans::TTrans(long a_nFileID)
{
    m_pStorage = G_dbManager.GetFile(a_nFileID);
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
BOOL FILE_InitStorage(long a_ndbID, TStorage *a_pStorage)
{
    FPOINTER    fpSegmentDir = 0;

    if (!a_pStorage)
        return FALSE;

    Security::TSecurityAttributes rootSecurity;
    a_pStorage->getRootSecurity(rootSecurity);
    rootSecurity.getBase()->protectionDomain = Util::UuidGenerate();

    TAdvPageRef    xPageRef(a_pStorage);

    FILEHEADER header = xPageRef.getHeader();

    if (header.m_fpSegmentDirPage == 0)
    {
        header.m_fpSegmentDirPage = TStoragePageAlloc::CreateNewPage(SEGID_SEGMENTDIR, FALSE);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }
    if (header.m_fpSecDataPage == 0)
    {
        header.m_fpSecDataPage = TStoragePageAlloc::CreateNewPage(SEGID_SECDATA, FALSE);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }

    if (header.m_RootSectionSegId == 0)
    {   
        header.m_RootSectionSegId = TSectionSegment::CreateSegment(a_ndbID, 0, TBATTR_NOVALUES, rootSecurity);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }
    if (header.m_LabelSectionSegId == 0)
    {
        header.m_LabelSectionSegId = TSectionSegment::CreateSegment(a_ndbID, 0, TBATTR_NOVALUES, &Security::Manager::LabelSecurity);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }
    if (header.m_TempLBSectionSegId == 0)
    {
        header.m_TempLBSectionSegId = TSectionSegment::CreateSegment(a_ndbID, 0, TBATTR_NOVALUES, &Security::Manager::SystemSecurity);
        header.m_bDirty = TRUE;
        xPageRef.setHeader(&header);
    }

    return TRUE;
};

//**********************************************************
BOOL TSegment::M_FixStorage()
{
    TDBFileEntry  *pEntry   = G_dbManager[m_ndbId];
    TStorage      *pStorage = pEntry ? pEntry->m_pStorage : NULL;
    DWORD          dwTime = GetTickCount(); 
    DWORD          dwTimeDiff = 0; 
    CFileStatus    status;

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
FPOINTER TSegment::GetPageByIndex(int a_nIndex)
{
    if(a_nIndex == 0)
        return m_fpFirst;

    if(a_nIndex >= GetPageCount())
        return 0;

    TSegmentPageIndexMap& segMapCache = G_segPageIndexMap.Value();

    FPOINTER fpPage = segMapCache.find(m_ndbId, m_nId, a_nIndex);
    if(fpPage == 0)
    {
        std::pair<int, FPOINTER> base = segMapCache.getLowerIndex(m_ndbId, m_nId, a_nIndex);
        if(base.first == 0)
            base.second = m_fpFirst;

        int nStart = a_nIndex & ~7;
        if(nStart < base.first)
            nStart = base.first;

        CArray<FPOINTER, FPOINTER> arrPages;

        if(!TStoragePageAlloc::GetNextPages(base.first, base.second, nStart, arrPages))
            return 0;

        for(int ctr = 0; ctr < arrPages.GetCount(); ctr++)
        {
            segMapCache(m_ndbId, m_nId, nStart + ctr) = arrPages[ctr];
        }

        fpPage = segMapCache.find(m_ndbId, m_nId, a_nIndex);
    }
    return fpPage;
}

//**********************************************************
void* TSegment::GetPageForRead(long a_nIndex, TAdvPageRef &a_PageRef)
{
    if(!M_FixStorage())
        return NULL;

    FPOINTER fpPage = GetPageByIndex(a_nIndex);
    if(fpPage == 0)
        return NULL;

    return a_PageRef.getPageForRead(fpPage);
};

//**********************************************************
void* TSegment::GetPageForWrite(long a_nIndex, TAdvPageRef &a_PageRef)
{
    if(!M_FixStorage())
        return NULL;

    FPOINTER fpPage = GetPageByIndex(a_nIndex);
    if(fpPage == 0)
        return NULL;

    return a_PageRef.getPageForWrite(fpPage);
};


//**********************************************************
long TSegment::AddNewPage(BOOL a_bReserve)
{
    long nCount = GetPageCount();

    if (nCount < 1)
        return -1;

    TSegmentPageIndexMap& segMapCache = G_segPageIndexMap.Value();

    M_FixStorage();
    std::pair<int, FPOINTER> lower = segMapCache.getLowerIndex(m_ndbId, m_nId, nCount);

    if(lower.first == 0)
        lower.second = m_fpFirst;

    FPOINTER fpNew = TStoragePageAlloc::AddNewPage(lower.second, m_nId, a_bReserve);
    if (fpNew == 0)
        return -1;

    m_nPageCount++;
    return nCount;
}

//**********************************************************
BOOL TSegment::Truncate(long a_nPage)
{
    FPOINTER fpPage = GetPageByIndex(a_nPage-1);
    if(fpPage == 0)
        return FALSE;

    m_nPageCount = -1; // reset
    TStoragePageAlloc::FreeDataChain(fpPage, TRUE);
    G_segPageIndexMap.Value().removeSegmentPages(m_ndbId, m_nId);
    return TRUE;
}

//**********************************************************
static BOOL FillSegmentEntry(SEGMENT_ENTRY *a_pEntry, long a_ndbId, long a_nSegmentId, const Security::TSecurityAttributes &a_secAttrs)
{
    FPOINTER  fpPage = 0;
    TTask    *pTask  = TASK_GetCurrent(); 

    a_pEntry->dwSegmentID = a_nSegmentId;
    a_pEntry->wRefCount   = 1;
    fpPage = TStoragePageAlloc::CreateNewPage(a_nSegmentId, TRUE);
    if (fpPage)
    {
        if(!a_secAttrs.getOwnerOverride())
        {
            Security::TTaskSecurityContext *pContext = Security::Manager::GetTaskContext();
            a_secAttrs.getBase()->owner = pContext->user;
        }
        a_pEntry->securityRef = Security::Manager::AddSecurity(a_ndbId, a_secAttrs);
        a_pEntry->dwFirstPage = fpPage;
        Security::TSegmentSecurityCache::getInstance().addSecurity(a_ndbId, a_nSegmentId, a_secAttrs.getBlock());
        return TRUE;
    }
    else
    {
        a_pEntry->dwSegmentID = 0;
    }
    return FALSE;
}

//**********************************************************
long SEGMENT_Create(long a_ndbId, const Security::TSecurityAttributes &a_secAttrs)
{
    TAdvPageRef  xPageRef(a_ndbId);
    long           nIdx           = 0;
    long           ctr            = 0;
    long           ctr2           = 0;
    long           nCount         = 0;
    long           nSegId         = 0; 
    SEGMENT_ENTRY   *pEntry         = NULL;
    BOOL           bExit          = FALSE;
    TDBFileEntry  *pFileEntry     = G_dbManager[a_ndbId];

    if(pFileEntry == NULL)
        return FALSE;

    xPageRef.lockGuardPageForWrite((FPOINTER)1);
    TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

    nCount = segmentDir.GetPageCount();
    for(ctr=0; ctr<nCount && !bExit; ctr++)
    {
        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForWrite(ctr, xPageRef);
        if(pEntry)
            for (ctr2=0; ctr2<SEGMENTS_PER_PAGE && !bExit; ctr2++)
            {
                if (pEntry[ctr2].dwSegmentID==0)
                {
                    nSegId = ctr * SEGMENTS_PER_PAGE + ctr2;
                    nSegId++;
                    if (FillSegmentEntry(&pEntry[ctr2], a_ndbId, nSegId, a_secAttrs))
                    {
                        bExit = true;
                    }
                }
            }
    }
    if (!bExit)
    {
        nIdx = segmentDir.AddNewPage();
        if (nIdx!=-1)
        {
            nSegId = 0;
            pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForWrite(nIdx, xPageRef);
            if(pEntry)
            {
                nSegId = nIdx * SEGMENTS_PER_PAGE;
                nSegId++;
                FillSegmentEntry(pEntry, a_ndbId, nSegId, a_secAttrs);
            }
        }
        else
        {
            nSegId = 0;
        }
    }
    return nSegId;
}

//**********************************************************
BOOL SEGMENT_AccessCheck(long a_ndbId, long a_nSegmentId, 
                         int a_minimalDesired, int a_maximalDesired, int &a_granted)
{
    TAdvPageRef  xPageRef(a_ndbId);
    xPageRef.lockGuardPageForRead((FPOINTER)1);

    Security::TSecurityAttributes secAttrs;

    if(!SEGMENT_GetSecurity(a_ndbId, a_nSegmentId, secAttrs))
        return FALSE;

    TDBFileEntry  *pFileEntry = G_dbManager[a_ndbId];
    TStorage      *pStorage      = pFileEntry ? pFileEntry->m_pStorage : NULL; 
    BOOL           bFileReadOnly = pStorage ? pStorage->isReadOnly() : TRUE;

    if(bFileReadOnly)
    {
        if(a_minimalDesired == TBACCESS_OPENDYNAMIC)
        {
            a_granted = TBACCESS_OPENDYNAMIC;
            return TRUE;
        }

        bool bTestOpenDynamic = (a_maximalDesired & TBACCESS_OPENDYNAMIC) == TBACCESS_OPENDYNAMIC;

        a_minimalDesired &= TBACCESS_READ;
        a_maximalDesired &= TBACCESS_READ;

        if(a_minimalDesired == 0)
            return FALSE;

        BOOL bResult = secAttrs.accessCheck(Security::Manager::GetTaskContext(), a_minimalDesired, a_maximalDesired, a_granted);
        if(bTestOpenDynamic)
            a_granted |= TBACCESS_OPENDYNAMIC;
        return bResult;
    }

    return secAttrs.accessCheck(Security::Manager::GetTaskContext(), a_minimalDesired, a_maximalDesired, a_granted);
}

//**********************************************************
BOOL SEGMENT_Assign(long a_ndbId, long a_nSegmentId, TSegment *a_pSegment)
{
    TAdvPageRef  xPageRef(a_ndbId);
    long           nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long           nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    SEGMENT_ENTRY   *pEntry = NULL;
    FPOINTER       fpPage = 0;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE, bTokenCompatible=FALSE, bOK=FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[a_ndbId];
    TStorage      *pStorage      = pFileEntry ? pFileEntry->m_pStorage : NULL; 
    BOOL           bFileReadOnly = pStorage ? pStorage->isReadOnly() : TRUE;

    if(pFileEntry == NULL)
        return FALSE;

    xPageRef.lockGuardPageForRead((FPOINTER)1);
    TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

    pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForRead(nPage, xPageRef);
    if(pEntry)
    {
        pEntry = &pEntry[nIndex];
        if (pEntry->dwSegmentID==DWORD(a_nSegmentId))
        {
            if (!a_pSegment->m_bAssigned)
            {
                fpPage = pEntry->dwFirstPage;
                a_pSegment->m_fpFirst = fpPage;
                a_pSegment->m_nId     = a_nSegmentId;
                a_pSegment->m_ndbId   = a_ndbId;
                a_pSegment->m_bAssigned = TRUE;
            }
            bRetVal = TRUE;
        }
    }
    return bRetVal;
}

//**********************************************************
BOOL SEGMENT_AddRef(long a_ndbId, long a_nSegmentId)
{
    TAdvPageRef  xPageRef(a_ndbId);
    long           nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long           nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    SEGMENT_ENTRY   *pEntry = NULL;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[a_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForWrite((FPOINTER)1);
        TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForWrite(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwSegmentID == DWORD(a_nSegmentId))
            {
                int granted = 0;
                if(Security::Manager::AccessCheck(a_ndbId, pEntry->securityRef, TBACCESS_LINK, TBACCESS_LINK, granted))
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
long SEGMENT_GetRefCount(long a_ndbId, long a_nSegmentId)
{
    TAdvPageRef   xPageRef(a_ndbId);
    long            nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long            nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    long            nRefCount = -1;   
    SEGMENT_ENTRY    *pEntry = NULL;
    TDBFileEntry   *pFileEntry = G_dbManager[a_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForRead((FPOINTER)1);
        TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwSegmentID==DWORD(a_nSegmentId))
            {
                nRefCount = pEntry->wRefCount;
            }

            return nRefCount;
        }
    }
    return -1;
}

//**********************************************************
BOOL SEGMENT_GetSecurity(long a_ndbId, long a_nSegmentId, Security::TSecurityAttributes & a_secAttrs)
{
    Security::SECURITY_ATTRIBUTES_BLOCK *pSecurity = Security::TSegmentSecurityCache::getInstance().findSecurity(a_ndbId, a_nSegmentId);
    if(pSecurity)
    {
        a_secAttrs.attach(pSecurity);
        return TRUE;
    }

    //=========================================================================
    TAdvPageRef   xPageRef(a_ndbId);
    long            nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long            nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    long            nRefCount = -1;   
    SEGMENT_ENTRY  *pEntry = NULL;
    TDBFileEntry   *pFileEntry = G_dbManager[a_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForRead((FPOINTER)1);
        TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwSegmentID==DWORD(a_nSegmentId))
            {
                if(Security::Manager::GetSecurity(a_ndbId, pEntry->securityRef, a_secAttrs))
                {
                    Security::TSegmentSecurityCache::getInstance().addSecurity(a_ndbId, a_nSegmentId, a_secAttrs.getBlock());
                    return TRUE;
                }
            }

            return FALSE;
        }
    }
    return FALSE;
}

//**********************************************************
BOOL SEGMENT_SetSecurity(long a_ndbId, long a_nSegmentId, const Security::TSecurityAttributes & a_secAttrs)
{
    TAdvPageRef   xPageRef(a_ndbId);
    long            nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long            nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    long            nRefCount = -1;   
    SEGMENT_ENTRY  *pEntry = NULL;
    TDBFileEntry   *pFileEntry = G_dbManager[a_ndbId];

    if(pFileEntry)
    {
        xPageRef.lockGuardPageForRead((FPOINTER)1);
        TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwSegmentID==DWORD(a_nSegmentId))
            {
                int granted = 0;

                if(Security::Manager::AccessCheck(a_ndbId, pEntry->securityRef, TBACCESS_SETSTATUS, TBACCESS_SETSTATUS, granted))
                {
                    Security::Manager::DeleteSecurity(a_ndbId, pEntry->securityRef);
                    pEntry->securityRef = Security::Manager::AddSecurity(a_ndbId, a_secAttrs);
                    Security::TSegmentSecurityCache::getInstance().addSecurity(a_ndbId, a_nSegmentId, a_secAttrs.getBlock());
                    return TRUE;
                }
            }

            return FALSE;
        }
    }
    return FALSE;
}

//**********************************************************
BOOL SEGMENT_SetSecurityForSection(long a_ndbIdParent, long a_nSegmentIdParent, 
                                   long a_ndbId, long a_nSegmentId, 
                                   TBSECURITY_OPERATION::Enum operation, 
                                   Security::TSecurityAttributes & a_secAttrs)
{
    int granted = 0;
    if(!SEGMENT_AccessCheck(a_ndbId, a_nSegmentId, TBACCESS_SETSTATUS, TBACCESS_SETSTATUS, granted))
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, NULL);
        return FALSE;
    }

    //-----------------------------------------------------
    Security::TSecurityAttributes secAttrs, parentSecAttrs;

    if(!SEGMENT_GetSecurity(a_ndbIdParent, a_nSegmentIdParent, parentSecAttrs))
        return FALSE;
    if(!SEGMENT_GetSecurity(a_ndbId, a_nSegmentId, secAttrs))
        return FALSE;

    //-----------------------------------------------------
    Security::Manager::MergeSecurity(operation, secAttrs, a_secAttrs);

    Util::Uuid parentProtectionDomain = parentSecAttrs.getBase()->protectionDomain;
    parentSecAttrs.getBase()->protectionDomain = Util::EmptyUuid;
    secAttrs.getBase()->protectionDomain = Util::EmptyUuid;

    Security::SECURITY_ATTRIBUTES_BLOCK *blk1 = parentSecAttrs.getBlock();
    Security::SECURITY_ATTRIBUTES_BLOCK *blk2 = secAttrs.getBlock();

    bool bEqual = false;
    int cbSize1 = TASK_MemSize(blk1);
    if(cbSize1 == TASK_MemSize(blk2))
        bEqual = memcmp(blk1, blk2, cbSize1) == 0;

    if(bEqual)
    {
        secAttrs.getBase()->protectionDomain = parentProtectionDomain;
    }
    else 
        secAttrs.getBase()->protectionDomain = Util::UuidGenerate();

    return SEGMENT_SetSecurity(a_ndbId, a_nSegmentId, secAttrs);
}

//**********************************************************
BOOL SEGMENT_SetSecurityForDomain(long a_ndbIdParent, long a_nSegmentIdParent, 
                                  long a_ndbId, long a_nSegmentId, 
                                  TBSECURITY_OPERATION::Enum operation, 
                                  Security::TSecurityAttributes & a_secAttrs)
{
    int granted = 0;
    if(!SEGMENT_AccessCheck(a_ndbId, a_nSegmentId, TBACCESS_SETSTATUS, TBACCESS_SETSTATUS, granted))
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, NULL);
        return FALSE;
    }

    //-----------------------------------------------------
    Security::TSecurityAttributes secAttrs, parentSecAttrs;

    if(!SEGMENT_GetSecurity(a_ndbIdParent, a_nSegmentIdParent, parentSecAttrs))
        return FALSE;
    if(!SEGMENT_GetSecurity(a_ndbId, a_nSegmentId, secAttrs))
        return FALSE;

    //-----------------------------------------------------
    if(secAttrs.getBase()->protectionDomain == parentSecAttrs.getBase()->protectionDomain)
    {
        TASK_SetErrorInfo(TRERR_CHANGING_CURRENT_DOMAIN, NULL, NULL);
        return FALSE;
    }

    return Security::Manager::SetSecurity(a_ndbId, secAttrs.getBase()->protectionDomain, operation, a_secAttrs);
}

//**********************************************************
BOOL SEGMENT_Delete(long a_ndbId, long a_nSegmentId)
{
    TAdvPageRef  xPageRef(a_ndbId);
    long           nPage  = (a_nSegmentId-1) / SEGMENTS_PER_PAGE;
    long           nIndex = (a_nSegmentId-1) % SEGMENTS_PER_PAGE;
    SEGMENT_ENTRY   *pEntry = NULL;
    TTask         *pTask  = TASK_GetCurrent();
    BOOL           bRetVal = FALSE, bAccept=FALSE;
    BOOL           bDeleted = FALSE;
    TDBFileEntry  *pFileEntry = G_dbManager[a_ndbId];

    if(pFileEntry)
    {
        G_segPageIndexMap.Value().removeSegmentPages(a_ndbId, a_nSegmentId);

        xPageRef.lockGuardPageForWrite((FPOINTER)1);
        TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(a_ndbId);

        pEntry = (SEGMENT_ENTRY *)segmentDir.GetPageForWrite(nPage, xPageRef);
        if(pEntry)
        {
            pEntry = &pEntry[nIndex];
            if (pEntry->dwSegmentID == DWORD(a_nSegmentId))
            {
                int granted = 0;
                if(Security::Manager::AccessCheck(a_ndbId, pEntry->securityRef, TBACCESS_DELETE, TBACCESS_DELETE, granted))
                {
                    if ((--pEntry->wRefCount)==0)
                    {
                        TStoragePageAlloc::FreeDataChain(pEntry->dwFirstPage, FALSE);
                        Security::Manager::DeleteSecurity(a_ndbId, pEntry->securityRef);

                        SEGID_F segId;
                        segId.ndbId = a_ndbId;
                        segId.nSegId = a_nSegmentId;
                        Junction::removeJunctionTarget(segId);

                        memset(pEntry, 0, sizeof(SEGMENT_ENTRY));
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
            FreeUnusedSegmentDirPages(&segmentDir);
        }
    }
    return bRetVal;
}

//**********************************************************
long SEGMENT_GetRootSection(long a_ndbId)
{
    TStorage   *pStorage = G_dbManager.GetFile(a_ndbId);
    FILEHEADER  header   = pStorage->getHeader();

    return header.m_RootSectionSegId;
}

//**********************************************************
long SEGMENT_GetLabelSection(long a_ndbId)
{
    TStorage    *pStorage = G_dbManager.GetFile(a_ndbId);
    FILEHEADER   header   = pStorage->getHeader();

    return header.m_LabelSectionSegId;
}

//**********************************************************
static void FreeUnusedSegmentDirPages(TSegment *a_pSegment)
{
    TAdvPageRef  xPageRef(a_pSegment->m_ndbId);
    long           nPage = 0, nPageCount = 0, ctr = 0;
    SEGMENT_ENTRY   *pEntry = NULL;
    long           nMaxUsed = 0;
    
    if (!a_pSegment)
        return;

    nPageCount = a_pSegment->GetPageCount();
    for (nPage=0; nPage<nPageCount; nPage++)
    {
        pEntry = (SEGMENT_ENTRY *)a_pSegment->GetPageForRead(nPage, xPageRef);
        if(pEntry)
        {
            for (ctr=0; ctr<SEGMENTS_PER_PAGE; ctr++)
            {
                if (pEntry[ctr].dwSegmentID!=0)
                {
                    nMaxUsed = nPage;
                    break;
                }
            }
        }
    }
    if (nMaxUsed<(nPageCount-1))
        a_pSegment->Resize(nMaxUsed+1);
}

//**********************************************************
int TSegment::GetPageCount()
{
    if(m_nPageCount == -1)
    {
        TStoragePageAlloc::GetNextPageCount(m_fpFirst, m_nPageCount);
    }
    return m_nPageCount;
}


//**********************************************************
BOOL TSegment::Resize(long a_nPageCount)
{
    assert(a_nPageCount > 0);

    long nCount = GetPageCount();
    long ctr = 0;
    BOOL bRetVal = FALSE;
    long nIndex  = 0;
    FPOINTER fpPage = 0;
    FPOINTER fpMax  = 0;

    if (nCount==a_nPageCount)
        return TRUE;

    M_FixStorage();
    if (nCount>a_nPageCount)
    {
        Truncate(a_nPageCount);
        return TRUE;
    }
    else
    {
        bRetVal = TRUE;
        nCount = a_nPageCount-nCount;
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

