#include "stdafx.h"
#include <afxtempl.h>
#include "TBaseFile.h"
#include "section.h"
#include "TTask.h"
#include "DataMgr.h"
#include "blob.h"
#include "label.h"
#include "lockpath.h"
#include "junction.h"
#include "stream.h"
#include "TSecurityAttributes.h"
#include "SecurityManager.h"
#include "TSystemStatus.h"
#include "TSectionSnapshot.h"

static TCriticalSection         G_cs;
static TAtomic32<TSectionData*> G_pSectionData = NULL;

static CMap<SEGID_F, SEGID_F&, TSectionData*, TSectionData*> G_mapSectionData;

//*************************************************************
long TIDArray::M_Add(long a_ndbId, long a_nSegId)
{
    SEGID_F segID = {a_ndbId, a_nSegId};
    return TIDArrayBase::Add(segID);
};

//*************************************************************
static void _SecPathCombine(CString &a_strDest, LPCSTR a_pszSource, LPCSTR a_pszName)
{
    int nLen = 0;

    a_strDest = a_pszSource;
    nLen = a_strDest.GetLength();
    if (nLen>0)
    {
        if (a_strDest[nLen-1]!='\\')
            a_strDest += '\\';
    }
    a_strDest += CString(a_pszName);
    nLen = a_strDest.GetLength();
    if (nLen>0)
    {
        if (a_strDest[nLen-1]!='\\')
            a_strDest += '\\';
    }
}

//*************************************************************
static BOOL _SecPathIsInUse(LPCSTR a_pszPath)
{
    TSectionData *pData    = NULL;
    TSectionName *pSecName = NULL;
    long          ctr=0, nCount=0;
    LPCSTR        pszPath = NULL; 
    long          nLen    = (a_pszPath ? strlen(a_pszPath) : 0);
    long          nLen2   = 0;

    TCSLock xCS(&G_cs);

    pData = G_pSectionData;
    //-------------------------------------
    while (pData)
    {
        nCount = pData->m_arrNames.GetSize();
        for (ctr=0; ctr<nCount; ctr++)
        {
            pSecName = &pData->m_arrNames[ctr];
            pszPath  = pSecName->m_strName;
            nLen2    = strlen(pszPath);
            if (nLen>0 && nLen<=nLen2)
            {
                if (strnicmp(a_pszPath, pszPath, nLen)==0)
                {
                    return TRUE;
                }
            }
        }
        pData = pData->m_pNext;
    }
    //-------------------------------------
    return FALSE;
}

//*************************************************************
static BOOL _WaitForPathUnused(LPCSTR a_pszPath)
{
    DWORD dwTick0 = GetTickCount();

    while (_SecPathIsInUse(a_pszPath))
    {
        Sleep(0);
        if (GetTickCount()-dwTick0 > 600)
            return FALSE;
    }
    return TRUE;
}

//*************************************************************
static TSectionData* _PollSectionData(SEGID_F a_segId)
{
    TSectionData *pData  = NULL;
    BOOL          bFound = FALSE;
    
    TCSLock _lock(&G_cs);

    if(!G_mapSectionData.Lookup(a_segId, pData))
    {
        return NULL;
    }

    return pData;
}

//*************************************************************
static TSectionData* _GetSectionData(SEGID_F a_segId)
{
    TSectionData *pData  = NULL;
    BOOL          bFound = FALSE;
    
    TCSLock _lock(&G_cs);

    if(!G_mapSectionData.Lookup(a_segId, pData))
    {
        pData = new TSectionData();
        G_mapSectionData.SetAt(a_segId, pData);
    }

    pData->m_nRefCount++;
    pData->m_segId = a_segId;
    return pData;
}

//*************************************************************
static BOOL _WaitForSegmentUnused(SEGID_F a_segId)
{
    DWORD dwTick0 = GetTickCount();

    while (_PollSectionData(a_segId))
    {
        Sleep(0);
        if (GetTickCount()-dwTick0 > 600)
            return FALSE;
    }
    return TRUE;
}

//*************************************************************
TSectionData::TSectionData(): 
    m_bLock(FALSE), m_nRefCount(0), m_nAttachCount(0), 
    m_nAttachWaiterCount(0), m_nLockWaiterCount(0), m_nAttachAndLockWaiterCount(0)
{
    TCSLock _lock(&G_cs);

    m_pNext = G_pSectionData;
    G_pSectionData = this;
}

//*************************************************************
TSectionData* TSectionData::AddRef()
{
    TCSLock _lock(&G_cs);

    m_nRefCount++;

    return this;
}

//*************************************************************
void TSectionData::Release()
{
    TCSLock _lock(&G_cs);

    if((--m_nRefCount) == 0)
    {
        G_mapSectionData.RemoveKey(m_segId);
        delete this;
    }
}

//*************************************************************
TSectionData::~TSectionData()
{
    TSectionData *pData     = G_pSectionData;
    TSectionData *pDataPrev = NULL;

    if (G_pSectionData==this)
    {
        G_pSectionData = m_pNext;
    }
    else
    {
        pDataPrev = pData;
        while (pData!=this)
        {
            pDataPrev = pData;
            pData = pData->m_pNext;
        }
        pDataPrev->m_pNext = m_pNext;
    }
}

//*************************************************************
BOOL TSectionData::Lock()
{
    TCSLock _cs(&m_cs);
    //------------------------------------------------------
    if(m_nAttachCount > 1 || m_bLock)
    {
        m_nLockWaiterCount++;
        if(m_hSemLock == NULL)
            m_hSemLock = CreateSemaphore(NULL, 0, 10, NULL);

        _cs.unlock();
        DWORD dwRes = WaitForSingleObject(m_hSemLock, 1500);
        _cs.lock();

        m_nLockWaiterCount--;
        if(dwRes != WAIT_OBJECT_0)
            return FALSE;
    }
    //------------------------------------------------------
    m_bLock = TRUE;
    return TRUE;
}

//*************************************************************
void TSectionData::Unlock()
{
    TCSLock _cs(&m_cs);
    //------------------------------------------------------
    m_bLock = FALSE;

    if(m_nLockWaiterCount)  // Lock by the same section
    {
        ReleaseSemaphore(m_hSemLock, 1, NULL);  // wake up first
        return;
    }

    if(m_nAttachWaiterCount)
        ReleaseSemaphore(m_hSemAttach, m_nAttachWaiterCount, NULL);  // wake up all
}

//*************************************************************
BOOL TSectionData::TaskAttachAndLock()
{
    TCSLock _cs(&m_cs);
    //------------------------------------------------------
    if(m_nAttachCount > 0)
    {
        m_nAttachAndLockWaiterCount++;
        if(m_hSemAttachAndLock == NULL)
            m_hSemAttachAndLock = CreateSemaphore(NULL, 0, 10, NULL);

        _cs.unlock();
        DWORD dwRes = WaitForSingleObject(m_hSemAttachAndLock, 1500);
        _cs.lock();

        m_nAttachAndLockWaiterCount--;
        if(dwRes != WAIT_OBJECT_0)
            return FALSE;
    }
    //------------------------------------------------------
    m_nAttachCount++;
    m_bLock = TRUE;
    return TRUE;
}

//*************************************************************
BOOL TSectionData::TaskAttach(BOOL a_bLock)
{
    if(a_bLock)
        return TaskAttachAndLock();

    TCSLock _cs(&m_cs);
    //------------------------------------------------------
    if(m_bLock)
    {
        m_nAttachWaiterCount++;
        if(m_hSemAttach == NULL)
            m_hSemAttach = CreateSemaphore(NULL, 0, 10, NULL);

        _cs.unlock();
        DWORD dwRes = WaitForSingleObject(m_hSemAttach, 1500);
        _cs.lock();

        m_nAttachWaiterCount--;
        if(dwRes != WAIT_OBJECT_0)
            return FALSE;
    }
    //------------------------------------------------------
    m_nAttachCount++;

    return TRUE;
}

//*************************************************************
void TSectionData::TaskDetach()
{
    TCSLock _cs(&m_cs);
    //------------------------------------------------------
    if ((--m_nAttachCount) == 1 && !m_bLock)
    {
        if(m_nLockWaiterCount)
            ReleaseSemaphore(m_hSemLock, 1, NULL);  // wake up first
    }
    if(m_nAttachCount == 0)
    {
        if(m_bLock)
        {
            m_bLock = FALSE;
            if(m_nAttachWaiterCount)
            {
                ReleaseSemaphore(m_hSemAttach, m_nAttachWaiterCount, NULL); // wake up all
                return;
            }
        }

        if(m_nAttachAndLockWaiterCount)
            ReleaseSemaphore(m_hSemAttachAndLock, 1, NULL); // wake up first
    }
}

//*********************************************************
void TSectionData::AddName(LPCSTR a_pszName)
{
    long          ctr      = 0;
    long          nCount   = 0;
    TSectionName *pSecName = NULL;
    TSectionName  secName;
    //------------------------------------------------------
    TCSLock xCS(&G_cs);

    nCount = m_arrNames.GetSize();
    //------------------------------------------------------
    for (ctr=0; ctr<nCount; ctr++)
    {
        pSecName = &m_arrNames[ctr];
        if (pSecName->m_strName.CompareNoCase(a_pszName)==0)
        {
            pSecName->m_nRefCount++;
            return;
        }
    }
    //------------------------------------------------------
    secName.m_nRefCount = 1;
    secName.m_strName = a_pszName;
    m_arrNames.Add(secName);
}

//*********************************************************
void TSectionData::RemoveName(LPCSTR a_pszName)
{
    long          ctr      = 0;
    long          nCount   = 0;
    TSectionName *pSecName = NULL;
    //------------------------------------------------------
    TCSLock xCS(&G_cs);

    nCount = m_arrNames.GetSize();
    //------------------------------------------------------
    for (ctr=0; ctr<nCount; ctr++)
    {
        pSecName = &m_arrNames[ctr];
        if (pSecName->m_strName.CompareNoCase(a_pszName)==0)
        {
            if ((--pSecName->m_nRefCount)==0)
            {
                m_arrNames.RemoveAt(ctr);
            };
            break;
        }
    }
    //------------------------------------------------------
}

//*********************************************************
BOOL TSectionData::AssignSegment(SEGID_F a_segId)
{
    TCSLock _lock(&m_cs);

    if(m_nAttachCount <= 1)
    {
        m_segId = a_segId;
        return SEGMENT_Assign(a_segId.ndbId, a_segId.nSegId, &m_seg);
    }
    else 
        return TRUE;
}

IMPLEMENT_TASK_MEMALLOC(TSection)

//*********************************************************
void TSection::Release()
{
    SEGID_F segID = {m_ndbId, m_nSegId};

    if (m_nOpenCount>0)
        ReleaseSectionData(segID);
}

//*********************************************************
TSection::~TSection()
{
    if(m_pSnapshot)
        delete m_pSnapshot;

    if(m_pIterator)
        delete m_pIterator;
}

//*********************************************************
BOOL TSection::WaitForSectionData(SEGID_F a_segId, LPCSTR a_pszName, TIDArray& a_arrIDs, BOOL a_bLock)
{
    int grantedAccess, desiredAccess = TBACCESS_BROWSE;

    if(a_bLock)
        desiredAccess |= TBACCESS_WRITE;

    if(!SEGMENT_AccessCheck(a_segId.ndbId, a_segId.nSegId, desiredAccess, TBACCESS_READWRITE, grantedAccess))
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
        return FALSE;
    }

    m_bReadOnly    = ((grantedAccess & TBACCESS_WRITE) == 0);
    m_bCanGetValue = ((grantedAccess & TBACCESS_GETVALUE) == TBACCESS_GETVALUE);

    //---------------------------------------------
    TSectionData *pData     = _GetSectionData(a_segId);
    BOOL          bRetVal   = FALSE;  
    BOOL          bAttachOK = FALSE;  

    //---------------------------------------------
    bAttachOK = pData->TaskAttach(a_bLock);
    if (bAttachOK)
    {
        bRetVal = pData->AssignSegment(a_segId);
        if (!bRetVal)
            TASK_SetErrorInfo(0, NULL, a_pszName);

        if(bRetVal && m_bReadOnly && a_bLock)
        {
            TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
            bRetVal = FALSE;   
        }
    }
    else
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, NULL, a_pszName);
    //---------------------------------------------
    if (!bRetVal)
    {
        if (bAttachOK)
            pData->TaskDetach();

        pData->Release();
        return FALSE;
    }
    //---------------------------------------------
    LOCKPATH_AddPath(a_pszName, a_arrIDs);
    m_pData = pData;
    return TRUE;
}

//*********************************************************
void TSection::ReleaseSectionData(SEGID_F a_segId)
{
    m_pData->TaskDetach();
    m_pData->Release();

    m_pData = NULL;
    LOCKPATH_RemovePath(m_strName, m_arrPathIDs);
}

//*********************************************************
BOOL TSection::BeginValueOperation(LPCSTR a_pszSubName)
{
    if(m_dwAttrs & TBATTR_NOVALUES)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszSubName, m_strName);
        return FALSE;
    }
    return TRUE;
}

//*********************************************************
BOOL TSection::InitialOpen(DWORD a_dwOpenMode)
{
    if(!Open())
        return FALSE;
    
    Storage::TAdvPageRef xPageRef(m_ndbId);

    TSectionSegment  *pSegment = GetSegment();

    TSectionSegment::Ref<SECTION_HEADER> headerP = pSegment->GetHeader(xPageRef);

    m_dwAttrs = headerP->m_dwAttrs;
    xPageRef.releasePage();

    if(a_dwOpenMode == TBOPEN_MODE_DEFAULT && (m_dwAttrs & TBATTR_NOVALUES) == 0)
    {
        int grantedAccess = 0;

        a_dwOpenMode
            = SEGMENT_AccessCheck(m_ndbId, m_nSegId, TBACCESS_OPENDYNAMIC, TBACCESS_OPENDYNAMIC, grantedAccess)
            ? TBOPEN_MODE_DYNAMIC
            : TBOPEN_MODE_SNAPSHOT;
    }

    if(m_dwAttrs & TBATTR_NOVALUES)
    {
        Close();
    }
    else if(a_dwOpenMode == TBOPEN_MODE_SNAPSHOT)
    {
        TSectionSnapshot *pSnapshot = TSectionSnapshot::create(pSegment);

        Close();

        if(!pSnapshot)
            return FALSE;

        m_pSnapshot = pSnapshot;
        m_ndbId = m_pSnapshot->dbId();
        m_nSegId = m_pSnapshot->segId();
        Open();

        m_bReadOnly = TRUE;
    }
    else // dynamic mode
    {
        int grantedAccess = 0;

        if(!SEGMENT_AccessCheck(m_ndbId, m_nSegId, TBACCESS_OPENDYNAMIC, TBACCESS_OPENDYNAMIC, grantedAccess))
        {
            Close();
            TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
            return FALSE;
        }
    }

    return TRUE;
}

//*********************************************************
BOOL TSection::Open(BOOL a_bLock)
{
    SEGID_F segID = {m_ndbId, m_nSegId};
    //----------------------------------------------------
    if (m_nOpenCount == 0)
    {
        if (!WaitForSectionData(segID, m_strName, m_arrPathIDs, a_bLock))
        {
            return FALSE;
        }
        m_nLock |= a_bLock;
        m_nOpenCount = 1;
        return TRUE;
    }
    //----------------------------------------------------
    m_nOpenCount++;
    m_nLock <<= 1;
    //----------------------------------------------------
    if(a_bLock)
    {
        if(!Lock())
        {
            m_nOpenCount--;
            m_nLock >>= 1;
            return FALSE;
        }
    }
    //----------------------------------------------------
    return TRUE;
};

//*********************************************************
BOOL TSection::Lock()
{
    if(m_pSnapshot)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
        return FALSE;
    }
    //-----------------------------------------------------
    TSectionSegment *pSegment = NULL;
    if (m_nLock == 0)
    {
        if (m_pData)
        {
            pSegment = GetSegment();
            if (m_bReadOnly)
            {
                TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
                return FALSE;
            }
            if (!m_pData->Lock())
            {
                TASK_SetErrorInfo(TRERR_SHARING_DENIED, NULL, m_strName);
                return FALSE;
            }
            m_nLock |= 1;
            return TRUE;
        }
        return FALSE;
    }
    m_nLock |= 1;
    return TRUE;
}

//*********************************************************
void TSection::Close()
{
    SEGID_F segId = {m_ndbId, m_nSegId};

    if ((--m_nOpenCount) == 0)
    {
        ReleaseSectionData(segId);
        if(m_pSnapshot)
            _release_ptr(m_pSnapshot);
        return;
    }
    else
    {
        Unlock();
    }
    m_nLock >>= 1;
}

//*********************************************************
void TSection::Unlock()
{
    if(m_pSnapshot)
        return;

    long nPrevLock = m_nLock;
    m_nLock &= ~1;
    if (nPrevLock && (m_nLock == 0))
    {
        if (m_pData)
            m_pData->Unlock();
    }
}


TSharedSection::RWLock TSharedSection::s_rwLock;

//*********************************************************
TRootSection::TRootSection()
{
    m_bSupportLabel = FALSE;
    m_nSegId = SEGMENT_GetRootSection(0);
    m_arrPathIDs.M_Add(m_ndbId, m_nSegId);
    m_strName = "\\";
    GetFirstPage();
};

//*********************************************************
TSectionSegment* TSection::GetSegment()
{
    if (m_pData)
        return &m_pData->m_seg;
    return NULL;
}

//*********************************************************
int TSection::CreateSectionSegment(DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs)
{
    std::auto_ptr<Security::TSecurityAttributes> defaultSecAttrs;

    if(a_pSecAttrs)
    {
        a_pSecAttrs->getBase()->protectionDomain = Util::UuidGenerate();
    }
    else
    {
        // inherit
        defaultSecAttrs = std::auto_ptr<Security::TSecurityAttributes>(new Security::TSecurityAttributes());
        SEGMENT_GetSecurity(m_ndbId, m_nSegId, *defaultSecAttrs);
        Security::Manager::InheritSecurity(*defaultSecAttrs);
    }

    return TSectionSegment::CreateSegment(
        m_ndbId, m_nSegId, a_dwAttrs, 
        (a_pSecAttrs ? *a_pSecAttrs : *defaultSecAttrs));
}

//*********************************************************
BOOL TSection::CreateSection(LPCSTR a_pszName, TSection *a_pSection, DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs, TSecCreateExtra *a_pExtra)
{
    Storage::TAdvPageRef   xPageRef(m_ndbId);
    BOOL                   bRetVal = FALSE;
    TSectionSegment       *pSegment  = NULL;
    BPOINTER               bpElem  = 0;
    LPVOID                 pvData  = NULL; 
    TBVALUE_ENTRY          value   = {0};
    TBVALUE_ENTRY         *pValue  = NULL;
    long                   nOffset = 0; 
    DWORD                  dwType  = 0; 
    long                   nSegId  = 0; 
    SEGID_F                segId   = {0,0};
    TSectionRef<>          refSec(this);

    //------------------------------------------------
    if(xPageRef.getStorage()->getFlags() & Storage::eFlag_DeleteOnly)
    {
        TASK_SetErrorInfo(TRERR_DISK_FULL, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    if(a_pSecAttrs && !Security::Manager::ValidateNewSecurity(*a_pSecAttrs))
    {
        TASK_SetErrorInfo(TRERR_INVALID_SECURITY, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    xPageRef.beginTrans();
    //------------------------------------------------
    if (!refSec.Open(TRUE))
        return FALSE;

    //------------------------------------------------
    pSegment = GetSegment();
    if (pSegment == NULL)
        return FALSE;

    bpElem = pSegment->FindValue(a_pszName);
    //------------------------------------------------
    if (bpElem != 0)
    {
        TSectionSegment::Ref<TBITEM_ENTRY> entryP = pSegment->GetItem(bpElem, xPageRef);
        dwType = TBITEM_TYPE(entryP.ptr());

        if (dwType != TBVTYPE_SECTION || a_pExtra)
        {
            TASK_SetErrorInfo(TRERR_CANNOT_CREATE_SECTION, a_pszName, m_strName);
            return FALSE;
        }
        else   //Segment ju¿ istnieje
        {
            pValue = TBITEM_VALUE(entryP.ptr());
            nSegId = pValue->int32;
            segId.nSegId = nSegId;
            segId.ndbId  = m_ndbId;
            TranslateSegmentID(segId);
            a_pSection->m_nSegId  = segId.nSegId;
            a_pSection->m_ndbId   = segId.ndbId;
            a_pSection->m_arrPathIDs.Copy(m_arrPathIDs);
            a_pSection->m_arrPathIDs.M_Add(m_ndbId, nSegId);
            _SecPathCombine(a_pSection->m_strName, m_strName, a_pszName);
            return a_pSection->InitialOpen(TBOPEN_MODE_DYNAMIC);
        }
    }
    //------------------------------------------------
    pSegment->Edit();
    bpElem = pSegment->AddValue(a_pszName, TBVTYPE_SECTION, &value);
    if (bpElem!=0)
    {
        if (a_pExtra) // U¿ywany tylko przy tworzeniu ³¹cza lub przenoszenia sekcji
        {
            nSegId = a_pExtra->nSegId;
            if (a_pExtra->bAddRef)
            {
                if (!SEGMENT_AddRef(m_ndbId, nSegId))
                    nSegId = 0;
            }
        }
        else
            nSegId = CreateSectionSegment(a_dwAttrs, a_pSecAttrs);
    }
    if (nSegId==0)
    {
        pSegment->CancelEdit();
        TASK_SetErrorInfo(TRERR_CANNOT_CREATE_SECTION, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    TSectionSegment::Ref<TBITEM_ENTRY> entryP = pSegment->GetItem(bpElem, xPageRef, TSectionSegment::eAccessWrite);
    pValue = TBITEM_VALUE(entryP.ptr());
    pValue->int32 = nSegId;
    pSegment->Update();
    //------------------------------------------------
    bRetVal = TRUE;
    if (a_pExtra==NULL)
    {
        a_pSection->m_arrPathIDs.Copy(m_arrPathIDs);
        a_pSection->m_arrPathIDs.M_Add(m_ndbId, nSegId);
        a_pSection->m_nSegId = nSegId;
        a_pSection->m_ndbId  = m_ndbId;
        _SecPathCombine(a_pSection->m_strName, m_strName, a_pszName);
        bRetVal = a_pSection->InitialOpen(TBOPEN_MODE_DYNAMIC);
    }
    //------------------------------------------------
    xPageRef.releaseAll();
    return bRetVal;
}

//*********************************************************
long TSection::FindSegmentItem(LPCSTR a_pszName, DWORD a_dwType)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);
    TSectionRef<>          refSec(this);

    TSectionSegment *pSegment  = NULL;
    BPOINTER       bpElem  = 0;
    LPVOID         pvData  = NULL; 
    TBITEM_ENTRY  *pEntry  = NULL;
    TBVALUE_ENTRY *pValue  = NULL;
    long           nOffset = 0; 
    DWORD          dwType  = 0; 
    long           nSegId  = 0; 
    //------------------------------------------------
    if (!refSec.Open())
        return 0;

    pSegment = GetSegment();
    bpElem = pSegment->FindValue(a_pszName);
    
    if (bpElem != 0)
    {
        pvData = pSegment->GetPageForRead(BP_PAGE(bpElem), xPageRef);
        nOffset = BP_OFS(bpElem);
        if (pvData)
        {
            pEntry = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
            dwType = TBITEM_TYPE(pEntry);
            if (dwType == a_dwType)
            {
                pValue = TBITEM_VALUE(pEntry);
                nSegId = pValue->int32;
            }
        }
    }
    //------------------------------------------------
    if (nSegId==0)
    {
        TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, a_pszName);
    }
    return nSegId;
}

//*********************************************************
SEGID_F TSection::ResolvePath(TSection *a_pBase, LPCSTR a_pszPath, CString &a_strFullPath, TIDArray *a_pPathIDs)
{
    TTask    *pTask = NULL;       
    CString   strPath(a_pszPath);
    CString   strName;
    CString   strSource;
    int       nLen   = 0;
    int       nPos   = 0;
    SEGID_F   segId  = {0,0};
    BOOL      bFound = FALSE;
    TSection  xSection;
    TSection *pSection = NULL;

    if (strPath.IsEmpty())
        return SEGID_NULL;
    //------------------------------------------------
    nLen = strPath.GetLength();
    if (nLen>1)
    {
        if (strPath[nLen-1]=='\\')
        {
            strPath.Delete(nLen-1);
        }
    }
    //------------------------------------------------
    if (strPath[0]=='\\')
    {
        pTask = TASK_GetCurrent();
        pSection = pTask->GetRootSection();
        strPath.Delete(0);
    }
    else if (strPath[0]=='~' && strPath[1]=='\\')
    {
        pTask = TASK_GetCurrent();
        pSection = pTask->GetPrimaryLabelSection();
        strPath.Delete(0, 2);
    }
    else
        pSection = a_pBase;
    //------------------------------------------------
    if(a_pPathIDs)
        a_pPathIDs->Copy(pSection->m_arrPathIDs);
    //------------------------------------------------
    do
    {
        nPos = strPath.Find('\\');
        if (nPos==-1)
        {
            strName = strPath;
            strPath.Empty();
        }
        else
        {
            strName = strPath.Left(nPos);
            strPath = strPath.Mid(nPos+1);
        }
        strSource = pSection->m_strName;
        bFound = pSection->FindSectionEx(strName, segId);
        if (!bFound)
        {
            TASK_SetErrorInfo(0, NULL, strSource);
            return SEGID_NULL;
        }
        if(a_pPathIDs)
            a_pPathIDs->Add(segId);
        if (strPath.IsEmpty())
        {
            _SecPathCombine(a_strFullPath, strSource, strName);
            return segId;
        }

        pSection = &xSection;
        pSection->m_nSegId = segId.nSegId;
        pSection->m_ndbId  = segId.ndbId;
        _SecPathCombine(pSection->m_strName, strSource, strName);
    }while(1);
    //------------------------------------------------
    return SEGID_NULL;
}

//*********************************************************
BOOL TSection::OpenSection(TSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode)
{
    CString strFullPath;
    SEGID_F segId = {0,0};

    m_arrPathIDs.RemoveAll();
    segId = ResolvePath(a_pBase, a_pszPath, strFullPath, &m_arrPathIDs);

    if (SEGID_IS_NULL(segId))
    {
        TASK_SetErrorInfo(TRERR_PATH_NOT_FOUND, a_pszPath, m_strName);
        return FALSE;
    }

    m_nSegId  = segId.nSegId;
    m_ndbId   = segId.ndbId;
    m_strName = strFullPath;
    return InitialOpen(a_dwOpenMode);
}

//*********************************************************
TSection* SECTION_CreateSection(TSection *a_pParent, LPCSTR a_pszName, DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs)
{
    TSection *pSection = new TSection();
    TTask    *pTask    = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    if (!a_pParent->CreateSection(a_pszName, pSection, a_dwAttrs, a_pSecAttrs))
    {
        delete pSection;
        return NULL;
    }
    Util::TTaskObjectSet<TSection> sset;
    sset.insert(pSection);

    return pSection;
};

//*********************************************************
TSection* SECTION_OpenSection(TSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode)
{
    TSection *pSection = new TSection();
    TTask    *pTask    = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    if (!pSection->OpenSection(a_pBase, a_pszPath, a_dwOpenMode))
    {
        delete pSection;
        return NULL;
    }

    Util::TTaskObjectSet<TSection> sset;
    sset.insert(pSection);

    return pSection;
}

//*********************************************************
BOOL SECTION_IsOpen(TSection *a_pSection)
{
    Util::TTaskObjectSet<TSection> sset;
    return sset.exists(a_pSection);
}

//*********************************************************
void SECTION_CloseSection(TSection *a_pSection)
{
    Util::TTaskObjectSet<TSection> sset;

    if (sset.exists(a_pSection))
    {
         a_pSection->Close();
        //-------------------------------------------
        if (a_pSection->GetOpenCount()==0)
        {
            a_pSection->Release();
            sset.removeLastFound();
        }
    }
}

//*********************************************************
void SECTION_RemoveAll()
{
    TTask           *pTask = TASK_GetCurrent();
    TSectionSetIter  iter;
    TSection        *pSection = NULL;

    if (pTask==NULL)
        return;

    Util::TTaskObjectSet<TSection> sset;
    sset.removeAll();
}

//*********************************************************
TSection* SECTION_FromHandle(HTBSECTION hSection)
{
    TTask    *pTask    = TASK_GetCurrent();
    TSection *pSection = (TSection*)hSection;

    if (hSection == TBSECTION_ROOT)
        return pTask->GetRootSection();

    Util::TTaskObjectSet<TSection> sset;
    pSection = sset.fromHandle(hSection);

    if(pSection == NULL)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER);
        return NULL;
    }

    return pSection;
}

//*********************************************************
BOOL TSection::ProcessBatch(LPVOID a_pvBatch)
{
    if(!BeginValueOperation(""))
        return NULL;
    //------------------------------------------------
    TSectionRef<>          refSec(this);
    Storage::TAdvPageRef xPageRef(m_ndbId);
    TSectionSegment *pSegment  = NULL;
    BOOL           bRetVal = FALSE; 
    //------------------------------------------------
    xPageRef.beginTrans();
    //------------------------------------------------
    if (!refSec.Open(TRUE))
        return FALSE;

    //------------------------------------------------
    pSegment = GetSegment();
    if (pSegment)
    {
        if (!pSegment->ProcessBatch(a_pvBatch, m_pIterator == NULL))
        {
            TASK_SetErrorInfo(0, NULL, m_strName);
            return FALSE;
        }
    }
    //------------------------------------------------
    return TRUE;
}

//*********************************************************
BOOL TSection::DeleteSegmentItem(LPCSTR a_pszName, long a_nSegId, BOOL a_bDeleteSegment)
{
    TSectionSegment         *pSegment  = NULL;
    BOOL                   bRetVal = FALSE;
    Storage::TAdvPageRef xPageRef(m_ndbId);  //top-level instance
    //------------------------------------------------
    pSegment = GetSegment();
    if (pSegment && pSegment->Edit())
    {   
        if (pSegment->DeleteValue(a_pszName))
        {
            bRetVal = TRUE;
            if (a_bDeleteSegment)
                bRetVal = SEGMENT_Delete(m_ndbId, a_nSegId);
            if (bRetVal)
            {
                if ((pSegment->GetDelCount() > MAXDELETED) && (m_pIterator == NULL))
                    pSegment->Compact();
                bRetVal = TRUE;
            }
            else
            {
                TASK_SetErrorInfo(0, a_pszName, m_strName);
            }
        }
    }

    //------------------------------------------------
    if (bRetVal)
    {
        pSegment->Update();
    }
    else
        pSegment->CancelEdit();
    //------------------------------------------------
    return bRetVal;
}

//*********************************************************
BOOL TSection::TranslateSegmentID(SEGID_F &a_segID)
{
    if(m_pSnapshot)
        m_pSnapshot->translateSegmentID(a_segID);
    
    if(!Junction::translateSegmentID(a_segID))
        return FALSE;

    TDBFileEntry *pEntry = G_dbManager[a_segID];

    if(pEntry)  //mounted file
    {
        if(pEntry->m_pStorage)
        {
            a_segID.ndbId  = pEntry->m_ndbID;
            a_segID.nSegId = SEGMENT_GetRootSection(a_segID.ndbId);
        }
        else
        {
            return FALSE;
        }
    }
    return TRUE;
}

//*********************************************************
BOOL TSection::FindSectionEx(LPCSTR a_pszName, SEGID_F &a_segID)
{
    long    nSegId = FindSection(a_pszName);

    if(nSegId==0)
    {
        return FALSE;
    }

    a_segID.nSegId = nSegId;
    a_segID.ndbId  = m_ndbId;
    if(!TranslateSegmentID(a_segID))
    {
        TASK_SetErrorInfo(TRERR_SECTION_NOT_MOUNTED, a_pszName, NULL);
        return FALSE;
    }
    return TRUE;
}

//*********************************************************
BOOL TSection::DeleteSection(LPCSTR a_pszName)
{
    TSectionRef<>          refSec(this);
    Storage::TAdvPageRef xPageRef(m_ndbId);   // top-level istance
    SEGID_F                segID = {0,0};
    CString                strPath;
    long                   nSegId = 0; 
    long                   nRefCount = 0;
    TSection               xSection;
    BOOL                   bRetVal= FALSE;
    BOOL                   bEmpty = FALSE;
    BOOL                   bFound = FALSE;
    DWORD                  dwError = 0;
    TSectionSegment         *pSegment  = NULL;

    //------------------------------------------------
    xPageRef.beginTrans();
    //------------------------------------------------
    if (!refSec.Open(TRUE))
        return FALSE;
    //------------------------------------------------
    _SecPathCombine(strPath, m_strName, a_pszName);

    segID.nSegId = m_nSegId;
    segID.ndbId  = m_ndbId;  
    if (!LOCKPATH_WaitForEntity(segID, a_pszName))
    {
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    bFound = FindSectionEx(a_pszName, segID);
    if (!bFound)
    {
        TASK_SetErrorInfo(0, NULL, m_strName);
        return FALSE;
    }
    else if(segID.ndbId != m_ndbId)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, a_pszName, m_strName);
        return FALSE;
    }
    nSegId = segID.nSegId;
    //------------------------------------------------
    if (!_WaitForSegmentUnused(segID))
    {
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    LABEL_EnsureLabels(m_ndbId);
    xSection.m_ndbId  = m_ndbId;
    xSection.m_nSegId = nSegId;
    xSection.m_strName = strPath;
    if (!xSection.Open())
        return FALSE;
    pSegment = xSection.GetSegment();
    nRefCount = SEGMENT_GetRefCount(m_ndbId, pSegment->m_nId);
    //------------------------------------------------
    if (nRefCount==-1)
    {
        bRetVal = FALSE;
    }
    else if (nRefCount>1)
    {
        bRetVal = TRUE;
        bEmpty  = TRUE;
    }
    else
        bRetVal = pSegment->IsEmpty(bEmpty);
    xSection.Close();
    //------------------------------------------------

    if (!bRetVal || !bEmpty)
    {
        bRetVal = FALSE;
        TASK_SetErrorInfo(TRERR_SECTION_NOT_EMPTY, a_pszName, m_strName);
        return FALSE;
    }
    bRetVal = DeleteSegmentItem(a_pszName, nSegId);
    if (bRetVal && nRefCount<=1)
        LABEL_DeleteSectionSegmentLabel(m_ndbId, nSegId);

    //------------------------------------------------
    xPageRef.releaseAll();
    return bRetVal;
}

//**********************************************************
LPVOID TSection::FetchValueByName(LPCSTR a_pszName, BOOL a_bSkipSections)
{
    LPVOID         pvData = NULL;
    TSectionSegment *pSegment = NULL;

    if (!Open())
        return NULL;

    if(!m_bCanGetValue)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
        return NULL;
    }

    pSegment = GetSegment();

    if (pSegment)
    {
        pvData = pSegment->FetchValueByName(a_pszName, a_bSkipSections);
        if (!pvData)
            TASK_SetErrorInfo(0, NULL, m_strName);
    }

    Close();
    return pvData;
}

//**********************************************************
ITERATED_ITEM_ENTRY* TSection::GetFirstItems(TBASE_ITEM_FILTER *a_pFilter)
{
    if(m_pIterator)
        _release_ptr(m_pIterator);

    m_pIterator = new TSectionSegmentIterator(a_pFilter);
    return GetNextItems();
}

//**********************************************************
ITERATED_ITEM_ENTRY* TSection::GetNextItems()
{
    if(m_pIterator == NULL)
        return NULL;

    //-----------------------------------------------------
    TSectionSegment *pSegment = NULL;
    ITERATED_ITEM_ENTRY* pItems = NULL;

    if (!Open())
        return NULL;

    pSegment = GetSegment();

    if (pSegment)
    {
        pItems = m_pIterator->getNextItems(pSegment);
        if (!pItems)
            TASK_SetErrorInfo(0, NULL, m_strName);
    }
    Close();

    if(m_pIterator->eof())
        _release_ptr(m_pIterator);

    return pItems;
}

//**********************************************************
LPVOID TSection::FetchValueByID(DWORD a_dwHash, WORD a_wOrder)
{
    LPVOID         pvData = NULL;
    TSectionSegment *pSegment = NULL;

    if (!Open())
        return NULL;

    pSegment = GetSegment();

    if (pSegment)
    {
        pvData = pSegment->FetchValueByID(a_dwHash, a_wOrder);
        if (!pvData)
            TASK_SetErrorInfo(0, NULL, m_strName);
    }

    Close();
    return pvData;
}

//**********************************************************
BOOL TSection::RenameSegmentItem(LPCSTR a_pszOldName, LPCSTR a_pszNewName, long a_nType, FN_WaitForSegmentUnused a_pfnWaitForSegmentUnused)
{
    TTrans         xTrans(m_ndbId);
    TSectionSegment *pSegment  = NULL;
    BOOL           bRetVal = FALSE;
    TBVALUE_ENTRY  valEntry = {0};
    long           nSegId  = 0;  

    pSegment = GetSegment();
    if (!pSegment)
        return FALSE;
    //----------------------------------------------------
    nSegId = FindSegmentItem(a_pszOldName, a_nType);
    if (nSegId==0)
        return FALSE;

    int granted;
    if(!SEGMENT_AccessCheck(m_ndbId, nSegId, TBACCESS_SETSTATUS, TBACCESS_SETSTATUS, granted))
    {
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszOldName, m_strName);
        return FALSE;
    }

    if (a_pfnWaitForSegmentUnused)
    {
        SEGID_F segId = {m_ndbId, nSegId};
        bRetVal = (*a_pfnWaitForSegmentUnused)(segId);
        if (!bRetVal)
        {
            TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszOldName, m_strName);
            return FALSE;
        }
    }

    valEntry.int32 = nSegId;
    bRetVal = (pSegment->ModifyValue(a_pszOldName, a_pszNewName, a_nType, &valEntry)!=0);
    if (!bRetVal)
    {
        TASK_SetErrorInfo(0, NULL, m_strName);
        return FALSE;
    }
    //----------------------------------------------------
    return TRUE;
}

//**********************************************************
BOOL TSection::RenameSection(LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    TSectionSegment *pSegment  = NULL;
    BOOL           bRetVal = FALSE; 
    SEGID_F        segId   = {0,0};  
    TBVALUE_ENTRY  valEntry = {0};
    CString        strPath;

    TTrans trans(m_ndbId);
    //----------------------------------------------------
    if (Open(TRUE))
    {
        _SecPathCombine(strPath, m_strName, a_pszOldName);

        segId.nSegId = m_nSegId;
        segId.ndbId  = m_ndbId;
        if (!LOCKPATH_WaitForEntity(segId, a_pszOldName))
        {
            TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszOldName, m_strName);
        }
        else
            bRetVal = RenameSegmentItem(a_pszOldName, a_pszNewName, TBVTYPE_SECTION);
        //----------------------------------------------------
        Close();
    }
    return bRetVal;
}

//**********************************************************
BOOL TSection::CreateLink(TSection *a_pBase, LPCSTR a_pszPath, LPCSTR a_pszLinkName)
{
    CString         strPath;
    SEGID_F         segId = ResolvePath(a_pBase, a_pszPath, strPath);
    TSection        xSection;
    TSecCreateExtra extra;
    BOOL            bRetVal = FALSE;

    if (SEGID_IS_NULL(segId))
    {
        TASK_SetErrorInfo(TRERR_PATH_NOT_FOUND, a_pszPath, m_strName);
        return FALSE;
    }
    if(segId.ndbId != m_ndbId)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszLinkName, m_strName);
        return FALSE;
    }
    int grantedAccess;
    if(!SEGMENT_AccessCheck(segId.ndbId, segId.nSegId, TBACCESS_LINK, TBACCESS_LINK, grantedAccess))
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, a_pszPath, NULL);
        return FALSE;
    }

    extra.bAddRef = TRUE;
    extra.nSegId  = segId.nSegId;

    xSection.m_nSegId  = segId.nSegId;
    xSection.m_ndbId   = segId.ndbId;
    xSection.m_strName = strPath;

    if (!xSection.Open())
        return FALSE;

    bRetVal = CreateSection(a_pszLinkName, NULL, 0, NULL, &extra);
    xSection.Close();

    return bRetVal;
}

//**********************************************************
BOOL TSection::GetSecurity(
        LPCSTR a_pszName,
        int    a_nSecurityInformationFlags, 
        std::auto_ptr<Util::Uuid> &a_owner,
        Util::Uuid                &a_protectionDomain,
        std::auto_ptr<Security::TSecurityAttributes> &a_attributes
    )
{
    SEGID_F segId;

    if(!FindSectionEx(a_pszName, segId))
        return FALSE;

    if(a_nSecurityInformationFlags == TBSECURITY_INFORMATION::eNONE)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER);
        return FALSE;
    }

    std::auto_ptr<Security::TSecurityAttributes> secAttrs(new Security::TSecurityAttributes());

    if(!SEGMENT_GetSecurity(segId.ndbId, segId.nSegId, *secAttrs))
    {
        System::setAlarm("DATABASE", Util::message("Cannot get security of section %s", m_strName));
        TASK_SetErrorInfo(TRERR_SYSTEM_ALARM);
        return FALSE;
    };

    if(a_nSecurityInformationFlags & TBSECURITY_INFORMATION::eOWNER)
    {
        a_owner.reset(new Util::Uuid());
        *a_owner = secAttrs->getBase()->owner;
    }
    if(a_nSecurityInformationFlags & TBSECURITY_INFORMATION::ePROTECTION_DOMAIN)
    {
        a_protectionDomain = secAttrs->getBase()->protectionDomain;
    }
    if(a_nSecurityInformationFlags & TBSECURITY_INFORMATION::eATTRIBUTES)
    {
        a_attributes = secAttrs;
    }
    return TRUE;
}

//*********************************************************
BOOL TSection::SetSecurity(
    LPCSTR a_pszName,
    TBSECURITY_TARGET::Enum a_target,
    TBSECURITY_OPERATION::Enum a_operation,
    Security::TSecurityAttributes &a_secAttrs)
{
    switch(a_operation)
    {
        case TBSECURITY_OPERATION::eREPLACE:
        case TBSECURITY_OPERATION::eADD:
        case TBSECURITY_OPERATION::eREMOVE:
            break;
        default:
            TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, a_pszName, m_strName);
            return FALSE;
    }
    //--------------------------------------------------
    SEGID_F segId;

    if(!FindSectionEx(a_pszName, segId))
        return FALSE;

    SEGID_F curSegId = {m_ndbId, m_nSegId};
    if (!LOCKPATH_WaitForEntity(curSegId, a_pszName))
    {
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszName, m_strName);
        return FALSE;
    }

    //--------------------------------------------------
    switch(a_target)
    {
        case TBSECURITY_TARGET::ePROTECTION_DOMAIN:
        {
            if(!SEGMENT_SetSecurityForDomain(m_ndbId, m_nSegId, segId.ndbId, segId.nSegId, a_operation, a_secAttrs))
            {
                TASK_SetErrorInfo(0, m_strName, a_pszName);
                return FALSE;
            }
            return TRUE;
        }
        case TBSECURITY_TARGET::eSECTION:
        {
            if(!SEGMENT_SetSecurityForSection(m_ndbId, m_nSegId, segId.ndbId, segId.nSegId, a_operation, a_secAttrs))
            {
                TASK_SetErrorInfo(0, m_strName, a_pszName);
                return FALSE;
            }
            return TRUE;
        }
    }

    //--------------------------------------------------
    TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
    return FALSE;
}    

//*********************************************************
BOOL TRootSection::ProcessBatch(LPVOID a_pvBatch)
{
    TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, NULL, m_strName);
    return FALSE;
}

//**************************************************************************
BOOL TRootSection::CanModifyLabel()
{
    return FALSE;
}

//*********************************************************
TLongBinary* TRootSection::CreateLongBinary(LPCSTR a_pszName, TSecCreateExtra *a_pExtra)
{
    TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName, m_strName);
    return FALSE;
}

//*********************************************************
void TSharedSection::GetFirstPage()
{
    TSegment seg;
    if(SEGMENT_Assign(m_ndbId, m_nSegId, &seg))
    {
        m_fpFirst = seg.m_fpFirst;
    }
}

//*********************************************************
void TSharedSection::Release()
{
    if (TSection::m_nOpenCount > 0)
        Close();
}

//*********************************************************
Stream::TOutputStream* TSection::CreateLongBinary(LPCSTR a_pszName, TSecCreateExtra *a_pExtra)
{
    if(!BeginValueOperation(a_pszName))
        return NULL;
    //------------------------------------------------

    BOOL                   bRetVal = FALSE;
    TSectionSegment       *pSegment  = NULL;
    BPOINTER               bpElem  = 0;
    LPVOID                 pvData  = NULL; 
    TBITEM_ENTRY          *pEntry  = NULL;
    TBVALUE_ENTRY          value   = {0};
    TBVALUE_ENTRY         *pValue  = NULL;
    long                   nOffset = 0; 
    DWORD                  dwType  = 0; 
    long                   nSegId  = 0;
    TLongBinary           *pLB     = NULL; 
    Storage::TAdvPageRef   xPageRef(m_ndbId);   // top-level istance
    TSectionRef<>          refSec(this);

    //------------------------------------------------
    if(xPageRef.getStorage()->getFlags() & Storage::eFlag_DeleteOnly)
    {
        TASK_SetErrorInfo(TRERR_DISK_FULL, a_pszName, m_strName);
        return FALSE;
    }
    //------------------------------------------------
    xPageRef.beginTrans();
    //------------------------------------------------
    if (!refSec.Open(TRUE))
        return FALSE;

    //------------------------------------------------
    pSegment = GetSegment();
    if (pSegment==NULL)
        return NULL;

    bpElem = pSegment->FindValue(a_pszName);

    //------------------------------------------------
    if (bpElem!=0)
    {
        nOffset = BP_OFS(bpElem);
        pvData = pSegment->GetPageForWrite(BP_PAGE(bpElem), xPageRef);
        if (pvData==NULL)
            return NULL;
        pEntry = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
        dwType = TBITEM_TYPE(pEntry);
        if (dwType != TBVTYPE_LONGBINARY || a_pExtra)
        {
            TASK_SetErrorInfo(TRERR_CANNOT_CREATE_BLOB, a_pszName, m_strName);
            return NULL;
        }
        else   //Segment ju¿ istnieje
        {
            pValue = TBITEM_VALUE(pEntry);
            nSegId = pValue->int32;
            pLB = TLongBinary::OpenLongBinary(this, nSegId);
            if(pLB)
            {
                pLB->Rewrite();
                return new TLongBinaryOutputStream(pLB);
            }
            return NULL;
        }
    }
    //------------------------------------------------
    pSegment->Edit();
    bpElem = pSegment->AddValue(a_pszName, TBVTYPE_LONGBINARY, &value);
    if (bpElem!=0)
    {
        if (a_pExtra) // U¿ywany tylko przy tworzeniu ³¹cza lub przenoszenia blobu
        {
            nSegId = a_pExtra->nSegId;
            if (a_pExtra->bAddRef)
            {
                if (!SEGMENT_AddRef(m_ndbId, nSegId))
                    nSegId = 0;
            }
        }
        else
            nSegId = SEGMENT_Create(m_ndbId, &Security::Manager::BlobSecurity); // Tylko 1 proces mo¿e u¿yc blobu w danej chwili
    }
    if (nSegId==0)
    {
        pSegment->CancelEdit();
        TASK_SetErrorInfo(TRERR_CANNOT_CREATE_SECTION, a_pszName, m_strName);
        return NULL;
    }
    //------------------------------------------------
    nOffset = BP_OFS(bpElem);
    pvData = pSegment->GetPageForWrite(BP_PAGE(bpElem), xPageRef);
    if (pvData==NULL)
    {
        pSegment->CancelEdit();
        return NULL;
    }
    pEntry = MAKE_PTR(TBITEM_ENTRY*, pvData, nOffset);
    pValue = TBITEM_VALUE(pEntry);
    pValue->int32 = nSegId;
    pSegment->Update();
    //------------------------------------------------
    bRetVal = TRUE;
    if (a_pExtra==NULL)
    {
        pLB = TLongBinary::OpenLongBinary(this, nSegId);
    }
    else
        return (Stream::TOutputStream*)(-1);
    //------------------------------------------------

    if (pLB!=NULL && a_pExtra==NULL)
        pLB->Rewrite();

    return (pLB ? new TLongBinaryOutputStream(pLB) : NULL);
}

//*********************************************************
Stream::TInputStream* TSection::OpenLongBinary(LPCSTR a_pszName)
{
    CString      strFullPath;
    long         nSegId = 0;
    TLongBinary *pLB = NULL;
    
    if (!Open())
        return NULL;

    if(!m_bCanGetValue)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName);
        return NULL;
    }

    nSegId = FindLongBinary(a_pszName);

    if (nSegId)
        pLB = TLongBinary::OpenLongBinary(this, nSegId);

    Close();

    return nSegId ? new TLongBinaryInputStream(pLB) : NULL;
}

//*********************************************************
BOOL TSection::DeleteLongBinary(LPCSTR a_pszName)
{
    long     nSegId = 0; 
    TSectionSegment  *pSegment  = NULL;
    TSectionRef<>   refSec(this);

    //------------------------------------------------
    if (!refSec.Open(TRUE))
        return FALSE;
    //------------------------------------------------
    nSegId = FindLongBinary(a_pszName);
    if (nSegId==0)
    {
        TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, m_strName, a_pszName);
        return FALSE;
    }
    //------------------------------------------------
    SEGID_F segId = {m_ndbId, nSegId};
    if (!TLongBinary::WaitForSegmentUnused(segId))
    {
        TASK_SetErrorInfo(TRERR_SHARING_DENIED, a_pszName, m_strName);
        return FALSE;
    }
    return DeleteSegmentItem(a_pszName, nSegId);
}

//**********************************************************
BOOL TSection::DeleteExistingLongBinary(LPCSTR a_pszName)
{
    if(FindLongBinary(a_pszName))
        return DeleteLongBinary(a_pszName);

    return TRUE;
}

//**********************************************************
BOOL TSection::RenameLongBinary(LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    TSectionSegment *pSegment  = NULL;
    BOOL           bRetVal = FALSE; 
    long           nSegId  = 0;  
    TBVALUE_ENTRY  valEntry = {0};
    CString        strPath;
    TSectionRef<>  secRef(this);

    //----------------------------------------------------
    if (!secRef.Open(TRUE))
        return FALSE;
    //----------------------------------------------------
    return RenameSegmentItem(a_pszOldName, a_pszNewName, TBVTYPE_LONGBINARY, &TLongBinary::WaitForSegmentUnused);
}

//**********************************************************
BOOL TSection::CreateLinkLB(TSection *a_pSource, LPCSTR a_pszValue, LPCSTR a_pszLinkName)
{
    int granted;

    if(!SEGMENT_AccessCheck(a_pSource->m_ndbId, a_pSource->m_nSegId, TBACCESS_LINK, TBACCESS_LINK, granted))
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, a_pSource->m_strName, a_pszValue);
        return FALSE;
    }
    //-----------------------------------------------------
    long            nSegId = a_pSource->FindLongBinary(a_pszValue);
    TSecCreateExtra extra;
    BOOL            bRetVal = FALSE;

    if (nSegId==0)
    {
        return FALSE;
    }

    if(m_ndbId != a_pSource->m_ndbId)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszLinkName, m_strName);
        return FALSE;
    }

    extra.bAddRef = TRUE;
    extra.nSegId  = nSegId;

    Stream::TInputStream *pLB = a_pSource->OpenLongBinary(a_pszValue);
    if (!pLB)
        return FALSE;
    Stream::TOutputStream *pLBX = CreateLongBinary(a_pszLinkName, &extra);
    pLB->close();

    return (pLBX!=NULL);
}

//**************************************************************************
Stream::TInputStream* SECTION_OpenLongBinary(TSection *a_pSection, LPCSTR a_pszName)
{
    Util::TTaskObjectSet<Stream::TInputStream> isset;

    return isset.insert( a_pSection->OpenLongBinary(a_pszName) );
}

//**************************************************************************
Stream::TOutputStream* SECTION_CreateLongBinary(TSection *a_pSection, LPCSTR a_pszName)
{
    Util::TTaskObjectSet<Stream::TOutputStream> osset;

    return osset.insert( a_pSection->CreateLongBinary(a_pszName) );
}


//**************************************************************************
BOOL TSection::SectionExists(TSection *a_pBase, LPCSTR a_pszPath, BOOL &a_bExist)
{
    CString strFullPath;
    SEGID_F segId = {0,0};
    TTask  *pTask = TASK_GetCurrent();
    BOOL    bRetVal = TRUE;

    segId = ResolvePath(a_pBase, a_pszPath, strFullPath);
    //------------------------------------------------
    if (SEGID_IS_NULL(segId))
    {
        bRetVal = FALSE;
        if (pTask->m_dwError==TRERR_ITEM_NOT_FOUND)
        {
            pTask->m_dwError = 0;
            pTask->m_strErrorItem.Empty();
            pTask->m_strErrorSection.Empty();
            bRetVal = TRUE;
        }
    }

    a_bExist = !SEGID_IS_NULL(segId);
    //------------------------------------------------
    return bRetVal;
}

//**************************************************************************
BOOL TSection::CanModifyLabel()
{
    CString strPath = m_strName;
    long    nLen    = -1;    

    if (m_bReadOnly)
        return FALSE;

    if (strPath[0]=='~')
    {
        strPath.Delete(0, 2);
        nLen    = strPath.GetLength();
        strPath.Delete(nLen-1);
        if (strPath.Find('\\')==-1)   //Jest to etykieta aktywnej sekcji
        {
            return FALSE;
        }
    }
    return TRUE;
};

//**************************************************************************
long TSection::GetLinkCount()
{
    long nLinkCount = -1;

    if (Open())
    {
        nLinkCount = SEGMENT_GetRefCount(m_ndbId, m_nSegId);
        Close();
    }
    return nLinkCount;
}

//************************************************************************
Util::TSectionPtr& Util::TSectionPtr::operator=(TSection *a_pSection)
{
    if(m_pSection)
        m_pSection->Close();
    m_pSection = a_pSection;

    return *this;
}

//************************************************************************
Util::TSectionPtr::~TSectionPtr()
{
    if(m_pSection)
        m_pSection->Close();
}

