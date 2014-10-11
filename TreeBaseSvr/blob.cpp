#include "stdafx.h"
#include <afxtempl.h>
#include "blob.h"
#include "section.h"
#include "DataMgr.h"
#include "TTask.h"
#include "TBaseFile.h"
#include "stream.h"
#include "InternalSection.h"
#include "SecurityManager.h"

template<> UINT AFXAPI HashKey<SEGID_F&>(SEGID_F& a_rSegIdf);

typedef struct
{
    DWORD    m_dwSize;
    FILETIME m_time;
}BLOB_HEADER;

static CRITICAL_SECTION                                    G_cs;
static CMap<SEGID_F, SEGID_F&, TLongBinary*, TLongBinary*> S_mapBlobMap;

//**************************************************************************
static BOOL InitModule()
{
    InitializeCriticalSection(&G_cs);
    return TRUE;
};

IMPLEMENT_INIT(InitModule)

//**************************************************************************
static TLongBinary* _PollLongBinary(SEGID_F a_nSegId)
{
    TLongBinary *pLB    = NULL;
    BOOL         bFound = FALSE;

    TCSLock xCS(&G_cs);
    //----------------------------------------------
    bFound = S_mapBlobMap.Lookup(a_nSegId, pLB);
    //----------------------------------------------
    return (bFound ? pLB : NULL);
}

//**************************************************************************
static TLongBinary* _GetLongBinary(SEGID_F a_nSegId)
{
    TLongBinary *pLB    = NULL;
    BOOL         bFound = FALSE;

    TCSLock xCS(&G_cs);
    //----------------------------------------------
    if(!S_mapBlobMap.Lookup(a_nSegId, pLB))
    {
        pLB = new TLongBinary();
        S_mapBlobMap.SetAt(a_nSegId, pLB);
    }
    //----------------------------------------------
    pLB->m_nRefCount++;
    return pLB;
}


//*************************************************************
static void _AddLongBinary(SEGID_F a_nSegId, TLongBinary* a_pLB)
{
    TCSLock xCS(&G_cs);

    S_mapBlobMap.SetAt(a_nSegId, a_pLB);

}

//*************************************************************
static void _RemoveLongBinary(SEGID_F a_nSegId)
{
    TCSLock xCS(&G_cs);

    S_mapBlobMap.RemoveKey(a_nSegId);
}


//**************************************************************************
TLongBinary::~TLongBinary()
{
};

//**************************************************************************
BOOL TLongBinary::Open(TSection *a_pSection, long a_nSegId)
{
    if(m_bOpen)
        return TRUE;

    TSectionRef<BlobSectionClose> refSec(a_pSection);

//    if (!a_pSection->Open(TRUE))
//        return FALSE;

    m_segId.ndbId  = a_pSection->m_ndbId;
    m_segId.nSegId = a_nSegId;

    if(!SEGMENT_Assign(a_pSection->m_ndbId, m_segId.nSegId, &m_seg))
    {
//        a_pSection->Close();
        return FALSE;
    }
    m_pSection = a_pSection;

    return TRUE;
}

//**************************************************************************
BOOL TLongBinary::Read(DWORD a_dwPos, DWORD &a_dwSize, LPVOID a_pvBuff)
{
    Storage::TAdvPageRef xPageRef(m_segId.ndbId);
    BLOB_HEADER*           pHeader  = NULL;
    DWORD                  dwPos    = a_dwPos + sizeof(BLOB_HEADER);
    DWORD                  dwPosMax = a_dwPos + a_dwSize + sizeof(BLOB_HEADER);
    long                   nPage0   = 0;
    long                   nOffset0 = 0;
    long                   nPage1   = 0;
    long                   nOffset1 = 0;
    BYTE                  *pByBuff  = (BYTE*)a_pvBuff;
    BYTE                  *pByPage  = NULL;
    long                   nPage=0, nMin=0, nMax=0, nSize=0;   
    BOOL                   bRetVal = FALSE;   

    //-------------------------------------------------------
    pHeader = (BLOB_HEADER*)m_seg.GetPageForRead(0, xPageRef);
    if (pHeader==NULL)
        return FALSE;
    if (dwPos>=pHeader->m_dwSize)
    {
        TASK_SetErrorInfo(TRERR_OUT_OF_DATA, m_strName, (m_pSection ? (LPCSTR)m_pSection->m_strName : NULL));
        return FALSE;
    }
    else if (dwPosMax > (pHeader->m_dwSize + sizeof(BLOB_HEADER)))
    {
        a_dwSize -= (dwPosMax-pHeader->m_dwSize-sizeof(BLOB_HEADER));
        dwPosMax = pHeader->m_dwSize + sizeof(BLOB_HEADER);
    }

    //-------------------------------------------------------
    nPage0   = dwPos / PAGESIZE;
    nOffset0 = dwPos % PAGESIZE; 
    nPage1   = dwPosMax / PAGESIZE;
    nOffset1 = dwPosMax % PAGESIZE; 
    //-------------------------------------------------------
    for (nPage=nPage0; nPage<=nPage1; nPage++)
    {
        nMin = (nPage==nPage0 ? nOffset0 : 0);
        nMax = (nPage==nPage1 ? nOffset1 : PAGESIZE);

        pByPage = (BYTE*)m_seg.GetPageForRead(nPage, xPageRef);
        if (pByPage == NULL)
            return FALSE;

        pByPage += nMin;
        nSize = nMax-nMin;
        memcpy(pByBuff, pByPage, nSize);
        pByBuff += nSize;

        xPageRef.lowCachingPriority() = true;
    }
    //-------------------------------------------------------
    return TRUE;
}

//**************************************************************************
BOOL TLongBinary::Write(DWORD a_dwPos, DWORD a_dwSize, LPVOID a_pvBuff)
{
    Storage::TAdvPageRef   xPageRef(m_segId.ndbId);
    BLOB_HEADER*           pHeader  = NULL;
    DWORD                  dwPos    = a_dwPos + sizeof(BLOB_HEADER);
    DWORD                  dwPosMax = a_dwPos + a_dwSize + sizeof(BLOB_HEADER);
    long                   nPage0   = 0;
    long                   nOffset0 = 0;
    long                   nPage1   = 0;
    long                   nOffset1 = 0;
    BYTE                  *pByBuff  = (BYTE*)a_pvBuff;
    BYTE                  *pByPage  = NULL;
    long                   nPage=0, nMin=0, nMax=0, nSize=0;   
    BOOL                   bRetVal = FALSE;   
    long                   nPageCount = 0;
    SYSTEMTIME             sysTime;

    GetSystemTime(&sysTime);
    //-------------------------------------------------------
    nPage0   = dwPos / PAGESIZE;
    nOffset0 = dwPos % PAGESIZE; 
    nPage1   = dwPosMax / PAGESIZE;
    nOffset1 = dwPosMax % PAGESIZE; 
    //-------------------------------------------------------
    nPageCount = m_seg.GetPageCount();
    if (nPageCount<(nPage1+1))
    {
        if (!m_seg.Resize(nPage1+1))
        {
            TASK_SetErrorInfo(TRERR_DISK_FULL, m_strName, m_pSection->m_strName);
            return FALSE;
        };
    }
    //-------------------------------------------------------
    for (nPage=nPage0; nPage<=nPage1; nPage++)
    {
        nMin = (nPage==nPage0 ? nOffset0 : 0);
        nMax = (nPage==nPage1 ? nOffset1 : PAGESIZE);

        pByPage = (BYTE*)m_seg.GetPageForWrite(nPage, xPageRef);
        if (pByPage==NULL)
            return FALSE;
        pByPage += nMin;
        nSize = nMax-nMin;
        memcpy(pByPage, pByBuff, nSize);
        pByBuff += nSize;

        xPageRef.lowCachingPriority() = true;
    }
    //-------------------------------------------------------
    pHeader = (BLOB_HEADER*)m_seg.GetPageForWrite(0, xPageRef);
    if (pHeader->m_dwSize < dwPosMax)
    {
        pHeader->m_dwSize = dwPosMax-sizeof(BLOB_HEADER);
    }
    SystemTimeToFileTime(&sysTime, &pHeader->m_time);
    //-------------------------------------------------------
    return TRUE;
}

//*************************************************************
BOOL TLongBinary::WaitForSegmentUnused(SEGID_F a_segId)
{
    TLongBinary *pLB = _PollLongBinary(a_segId);
    if(pLB == NULL)
        return TRUE;

    TCSLock _lock(&G_cs);

    if(pLB->m_hSemUnused == NULL)
        pLB->m_hSemUnused = CreateSemaphore(NULL, 0, 10, NULL);

    pLB->m_nUnusedWaiterCount++;
    _lock.unlock();
    DWORD dwRes = WaitForSingleObject(pLB->m_hSemUnused, 600);
    _lock.lock();
    pLB->m_nUnusedWaiterCount--;

    return dwRes == WAIT_OBJECT_0;
}

//**************************************************************************
BOOL TLongBinary::WaitForAttach()
{
    TCSLock _lock(&G_cs);
    m_nAttachWaiterCount++;

    _lock.unlock();
    DWORD dwRes = WaitForSingleObject(m_hSem, 1500);
    _lock.lock();

    m_nAttachWaiterCount--;
    if(dwRes == WAIT_OBJECT_0)
    {
        m_bAttached = TRUE;
        return TRUE;
    }

    return FALSE;
}

//**************************************************************************
TLongBinary* TLongBinary::OpenLongBinary(TSection *a_pSection, long a_nSegId)
{
    SEGID_F      segId = {a_pSection->m_ndbId, a_nSegId};
    TLongBinary *pLB   = _GetLongBinary(segId);
    TTask       *pTask = TASK_GetCurrent();

    //-------------------------------------------------------
    if (!pLB->WaitForAttach())
    {
        TLongBinary::CloseLongBinary(pLB);
        return NULL;
    }
    //-------------------------------------------------------
    if (!pLB->Open(a_pSection, a_nSegId))
    {
        TLongBinary::CloseLongBinary(pLB);
        return NULL;
    }
    return pLB;
}

//**************************************************************************
void TLongBinary::CloseLongBinary(TLongBinary* a_pLB)
{
    TCSLock _lock(&G_cs);

    if(a_pLB->m_bAttached)
    {
        ReleaseSemaphore(a_pLB->m_hSem, 1, NULL);
        a_pLB->m_bAttached = FALSE;
    }

    if ((--a_pLB->m_nRefCount) == 0)
    {
//        if(a_pLB->m_pSection)
//        {
//            a_pLB->m_pSection->Unlock();
//            SECTION_CloseSection(a_pLB->m_pSection);
//        }
        _RemoveLongBinary(a_pLB->m_segId);
        if(a_pLB->m_nUnusedWaiterCount)
            ReleaseSemaphore(a_pLB->m_hSemUnused, a_pLB->m_nUnusedWaiterCount, NULL); // wake up all
        delete a_pLB;
    }

}

//****************************************************************
BOOL TLongBinary::Rewrite()
{
    Storage::TAdvPageRef xPageRef(m_segId.ndbId);
    BLOB_HEADER*         pHeader  = NULL;
    SYSTEMTIME           sysTime;
    
    GetSystemTime(&sysTime);

    if (m_seg.GetPageCount()>1)
        m_seg.Resize(1);

    pHeader = (BLOB_HEADER*)m_seg.GetPageForWrite(0, xPageRef);
    if (pHeader==NULL)
    {
        return FALSE;
    }
    memset(pHeader, 0, PAGESIZE);
    SystemTimeToFileTime(&sysTime, &pHeader->m_time);

    return TRUE;
}

//**************************************************************************
BOOL TLongBinary::GetStatus(LBSTATUS *a_pStatus)
{
    Storage::TAdvPageRef xPageRef(m_segId.ndbId);
    BLOB_HEADER*           pHeader = NULL;
    BOOL                   bRetVal = FALSE;
    //-------------------------------------------------------
    pHeader = (BLOB_HEADER*)m_seg.GetPageForRead(0, xPageRef);
    
    if (pHeader)
    {
        a_pStatus->dwSize = pHeader->m_dwSize;
        a_pStatus->mTime  = pHeader->m_time;
        bRetVal = TRUE;
    }
    //-------------------------------------------------------
    return bRetVal;
}

//*********************************************************
TLongBinaryInputStream::TLongBinaryInputStream(TLongBinary *a_pLB): 
    m_pLB(a_pLB),
    m_ofs(0)
{
    memset(&m_status, 0, sizeof(m_status));
}

//*********************************************************
TLongBinaryInputStream::~TLongBinaryInputStream() 
{
    close();
};

//*********************************************************
void* TLongBinaryInputStream::read(DWORD a_cbSize)
{
    if(m_status.dwSize == 0 && (m_status.mTime.dwLowDateTime == 0))
    {
        if(!m_pLB->GetStatus(&m_status))
            return 0;
    }
    a_cbSize = min(a_cbSize, m_status.dwSize - m_ofs);

    Util::TTaskAutoPtr<void> buff(TASK_MemAlloc(a_cbSize));

    if(m_pLB->Read(m_ofs, a_cbSize, buff))
    {
        m_ofs += a_cbSize;
        return buff.detach();
    }
    else
        return NULL;
}

//*********************************************************
void TLongBinaryInputStream::close()
{
    if(m_pLB)
    {
        TLongBinary::CloseLongBinary(m_pLB);
        m_pLB = NULL;
    }
}

//*********************************************************
TLongBinaryOutputStream::TLongBinaryOutputStream(TLongBinary *a_pLB): 
    m_pLB(a_pLB), m_ofs(0)
{
}

//*********************************************************
TLongBinaryOutputStream::~TLongBinaryOutputStream() 
{
    close();
};

//*********************************************************
DWORD TLongBinaryOutputStream::write(void *a_pvBuff, DWORD a_cbSize)
{
    if(m_pLB->Write(m_ofs, a_cbSize, a_pvBuff))
    {
        m_ofs += a_cbSize;
        return a_cbSize;
    }
    else
        return 0;
}

//*********************************************************
bool TLongBinaryOutputStream::close()
{
    if(m_pLB)
    {
        TLongBinary::CloseLongBinary(m_pLB);
        m_pLB = NULL;
    }
    return true;
}

//*********************************************************
bool Stream::close(Stream::TInputStream *a_pStream)
{
    Util::TTaskObjectSet<Stream::TInputStream> isset;

    if (isset.exists(a_pStream))
    {
        a_pStream->close();
        isset.removeLastFound();
    }
    return true;
}

//*********************************************************
bool Stream::close(Stream::TOutputStream *a_pStream)
{
    Util::TTaskObjectSet<Stream::TOutputStream> osset;

    if (osset.exists(a_pStream))
    {
        bool bOK = a_pStream->close();
        osset.removeLastFound();
        return bOK;
    }
    return false;
}


//*********************************************************
void BLOB_RemoveUncommitted(TSection *a_pTempSection)
{
    Util::Internal::TSectionIterator itTempSection(a_pTempSection);

    ITERATED_ITEM_ENTRY *pSubEntry = itTempSection.getNext();

    while(pSubEntry)
    {
        Util::TSectionPtr subSec = SECTION_OpenSection(a_pTempSection, pSubEntry->szName, TBOPEN_MODE_DYNAMIC);
        if(!subSec) continue;

        {
            Util::Internal::TSectionIterator itLBSection(subSec);

            ITERATED_ITEM_ENTRY *pLBEntry = itLBSection.getNext();
            subSec->DeleteLongBinary("LB");
            subSec = NULL;
        }        
        a_pTempSection->DeleteSection(pSubEntry->szName);

        pSubEntry = itTempSection.getNext();
    }
}

//*********************************************************
TSection* BLOB_CreateBlobTempSection(TSection *a_pSource, LPCSTR a_pszName)
{
    Security::TPrivilegedScope privileged;

    CString strPath;
    strPath.Format("\\System\\Temp\\DB%d", a_pSource->m_ndbId);

    Util::TSectionPtr dbxSection = SECTION_OpenSection(NULL, strPath, TBOPEN_MODE_DYNAMIC);
    if(!dbxSection)
        return NULL;

    Security::TSecurityAttributes secAttrs(TBACCESS_ALL, 0, TBACCESS_ALL);

    return SECTION_CreateSection(dbxSection, a_pszName, 0, NULL);
}

//*********************************************************
BOOL BLOB_CommitBlob(TSection *a_pSection, LPCSTR a_pszName, LPCSTR a_pszTempSection)
{
    Storage::TAdvPageRef xPageRef(a_pSection->m_ndbId);

    if(a_pSection->FindLongBinary(a_pszName))
    {
        if(!a_pSection->DeleteLongBinary(a_pszName))
            return FALSE;
    }

    Security::TPrivilegedScope privileged;

    CString strTempPath, strPath;
    strTempPath.Format("\\System\\Temp\\DB%d", a_pSection->m_ndbId);

    strPath = strTempPath;
    strPath += "\\";
    strPath += a_pszTempSection;

    Util::TSectionPtr tempSection = SECTION_OpenSection(NULL, strPath, TBOPEN_MODE_DYNAMIC);
    if(!tempSection)
        return FALSE;

    if(!a_pSection->CreateLinkLB(tempSection, "LB", a_pszName))
        return FALSE;

    tempSection->DeleteLongBinary("LB");
    tempSection = SECTION_OpenSection(NULL, strTempPath, TBOPEN_MODE_DYNAMIC);
    tempSection->DeleteSection(a_pszTempSection);

    return TRUE;
}
