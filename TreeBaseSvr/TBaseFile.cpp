// TBaseFile.cpp: implementation of the TBaseFile class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TBaseFile.h"
#include "TTask.h"
#include "SegmentMgr.h"
#include "Label.h" 
#include "TSystemStatus.h"
#include "junction.h"
#include <shlwapi.h>
#include "blob.h"
#include "SecurityManager.h"

static CRITICAL_SECTION G_cs;

static const SEGID_F MOUNT_SEGID_TEMP = {-1, -1};

int FILE_dbIdTemp = -1;

//*************************************************************************
static BOOL Initialize()
{
    InitializeCriticalSection(&G_cs);
    return TRUE;
}

IMPLEMENT_INIT(Initialize)

//*************************************************************************
void FILE_Lock()
{
    EnterCriticalSection(&G_cs);
}

//*************************************************************************
void FILE_Unlock()
{
    LeaveCriticalSection(&G_cs);
}

//************************************************************************
BOOL FILE_InitTempDatabase()
{
    long idx = G_dbManager.AddEntry(MOUNT_SEGID_TEMP, "", "$TEMP");
    if(G_dbManager.MountFile(idx) == NULL)
        return FALSE;

    FILE_dbIdTemp = idx;
    return TRUE;
}

//************************************************************************
int FILE_MountDatabase(SEGID_F a_segMntPoint, LPCSTR a_pszName, LPCSTR a_pszFile)
{
    long idx = G_dbManager.AddEntry(a_segMntPoint, a_pszName, a_pszFile);

    if (G_dbManager.MountFile(idx) == NULL)
        return -1;

    CString strTemp("\\System\\Temp");
    Util::TSectionPtr tempSection = SECTION_OpenSection(NULL, strTemp, TBOPEN_MODE_DYNAMIC);
    if(!tempSection)
        return -1;

    CString strDBx;
    strDBx.Format("DB%d", idx);

    Security::TSecurityAttributes tempSecurity(&Security::Manager::TempSecurity);

    Util::TSectionPtr dbxSection = SECTION_CreateSection(tempSection, strDBx, TBATTR_NOVALUES, &tempSecurity);
    if(!dbxSection)
        return -1;

    SEGID_F srcSegID = {dbxSection->m_ndbId, dbxSection->m_nSegId};

    dbxSection = NULL;
    tempSection = NULL;

    Storage::TStorage *pStorage = G_dbManager.GetFile(idx);
    FILEHEADER  header   = pStorage->getHeader();

    SEGID_F destSegID = {idx, header.m_TempLBSectionSegId};

    Junction::addGlobalJunctionPoint(srcSegID, destSegID);
    
    strTemp += "\\";
    strTemp += strDBx;
    tempSection = SECTION_OpenSection(NULL, strTemp, TBOPEN_MODE_DYNAMIC);
    if(!tempSection)
        return -1;

    BLOB_RemoveUncommitted(tempSection);

    return idx;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//*************************************************************************
void TDBManager::LockShared()
{
    TCSLock _lock(&m_cs);

    if(m_bExclusiveLock)
    {
        m_nSharedLockWaiters++;

        _lock.unlock();
        WaitForSingleObject(m_hSemLockShared, INFINITE);
        _lock.lock();

        m_nSharedLockWaiters--;
    }
    m_nSharedLockCount++;
}

//*************************************************************************
void TDBManager::UnlockShared()
{
    TCSLock _lock(&m_cs);

    if(--m_nSharedLockCount == 0)
    {
        if(m_nExclusiveLockWaiters > 0)
            ReleaseSemaphore(m_hSemLockExclusive, 1, NULL);
    }
}

//*************************************************************************
void TDBManager::LockExclusive()
{
    TCSLock _lock(&m_cs);

    if(m_bExclusiveLock || m_nSharedLockCount > 0)
    {
        m_nExclusiveLockWaiters++;

        _lock.unlock();
        WaitForSingleObject(m_hSemLockExclusive, INFINITE);
        _lock.lock();

        m_nExclusiveLockWaiters--;
    }
    m_bExclusiveLock = true;
}

//*************************************************************************
void TDBManager::UnlockExclusive()
{
    TCSLock _lock(&m_cs);

    m_bExclusiveLock = false;

    if(m_nSharedLockWaiters > 0)
    {
        ReleaseSemaphore(m_hSemLockShared, m_nSharedLockWaiters, NULL);
    }
    else if(m_nExclusiveLockWaiters > 0)
    {
        ReleaseSemaphore(m_hSemLockExclusive, 1, NULL);
    }
}

//*************************************************************************
long TDBManager::AddEntry(SEGID_F a_segMntPoint, LPCSTR a_pszName, LPCSTR a_pszFile)
{
    TCSLock _lock(&m_cs);

    TDBFileEntry *pEntry = new TDBFileEntry;
    long          nID   = 0;

    if(pEntry)
    {
        pEntry->m_nRefs       = 0;
        pEntry->m_segMntPoint = a_segMntPoint;
        pEntry->m_strFile     = a_pszFile;
        pEntry->m_strName     = a_pszName;
        pEntry->m_pStorage    = NULL;
        pEntry->m_bCheckMedia = FALSE;
        pEntry->m_dwLastAccessTime = 0;
        RWLock_Initialize(&pEntry->m_rwLock);

        nID = m_arrEntries.Add(pEntry);
        m_mapBySegID.SetAt(a_segMntPoint, nID);
        pEntry->m_ndbID = nID;
        return nID;
    }
    return -1;
}

//*************************************************************************
TDBFileEntry* TDBManager::operator[](long a_nID)
{
    TCSLock _lock(&m_cs);

    long nCount = m_arrEntries.GetSize();

    if(a_nID<0 || a_nID>=nCount)
        return NULL;
    return m_arrEntries[a_nID];
}

//*************************************************************************
TDBFileEntry* TDBManager::operator[](SEGID_F &a_segMntPoint)
{
    TCSLock _lock(&m_cs);

    long nID = 0;

    if(m_mapBySegID.Lookup(a_segMntPoint, nID))
        return m_arrEntries[nID];
    return NULL;
}

//*************************************************************************
Storage::TStorage* TDBManager::GetFile(long a_nID)
{
    TDBFileEntry *pEntry = operator[](a_nID);

    if(pEntry)
        return pEntry->m_pStorage;
    return NULL;
}

//*************************************************************************
Storage::TStorage* TDBManager::MountFile(long a_nID)
{
    TCSLock _lock(&m_cs);

    BOOL               bOK       = FALSE;
    TSegment          *pSegmentDir = NULL;   
    Storage::TStorage *pStorage  = NULL;
    TDBFileEntry      *pEntry    = operator[](a_nID);
    TLabelManagerBase *pLabelMgr = NULL;

    if(pEntry)
    {
        pSegmentDir = new TSegment();
        pStorage  = Storage::TStorage::open(pEntry->m_strFile);
        pLabelMgr = LABEL_CreateManager();
        if(pStorage && pSegmentDir && pLabelMgr)
        {
            InterlockedExchange((LPLONG)&pEntry->m_pStorage,      (LONG)pStorage);
            InterlockedExchange((LPLONG)&pEntry->m_pLabelManager, (LONG)pLabelMgr);
            bOK = FILE_InitStorage(a_nID, pStorage);
        }
    }
    if(bOK)
    {
        pEntry->m_strStatusKey = System::addDatabase(pEntry->m_strFile);
        return pStorage;
    }
    else
    {
        _release_ptr(pStorage);
        _release_ptr(pSegmentDir);
        if(pLabelMgr)
        {
            pLabelMgr->Delete();
            pLabelMgr = NULL;
        }
        InterlockedExchange((LPLONG)&pEntry->m_pStorage,      0);
        InterlockedExchange((LPLONG)&pEntry->m_pLabelManager, 0);
    }
    return NULL;
}

//*************************************************************************
BOOL TDBManager::UmountFile(long a_nID)
{
    TCSLock _lock(&m_cs);

    TDBFileEntry      *pEntry    = operator[](a_nID);
    Storage::TStorage *pStorage  = NULL;
    TSegment            *pSegmentDir = NULL;
    TLabelManagerBase *pLabelMgr = NULL;

    if(pEntry && pEntry->m_nRefs==0)
    {
        System::removeDatabase(pEntry->m_strFile);

        pStorage  = pEntry->m_pStorage;
        pLabelMgr = pEntry->m_pLabelManager;
        InterlockedExchange((LPLONG)&pEntry->m_pStorage,  0);
        InterlockedExchange((LPLONG)&pEntry->m_pLabelManager, 0);
        pStorage->close();
        delete pSegmentDir;
        pLabelMgr->Delete();
        return TRUE;
    }
    return FALSE;
}

//*************************************************************************
BOOL TDBManager::IncFileRefs(long a_nID)
{
    TDBFileEntry *pEntry = operator[](a_nID);
    if(pEntry)
    {
        InterlockedIncrement(&pEntry->m_nRefs);
        return TRUE;
    }
    return FALSE;
}

//*************************************************************************
BOOL TDBManager::DecFileRefs(long a_nID)
{
    TDBFileEntry *pEntry = operator[](a_nID);
    if(pEntry)
    {
        InterlockedDecrement(&pEntry->m_nRefs);
        return TRUE;
    }
    return FALSE;
}

TDBManager G_dbManager;

//**********************************************************
static BOOL _FILE_Mount(long a_nCode, WPARAM a_wParam, LPARAM a_lParam)
{
    LPCSTR        pszDBase = (LPCSTR)a_wParam;
    long          ctr      = 0;
    long          nCount   = 0;
    TDBFileEntry *pEntry   = NULL;
    BOOL          bCreate  = (BOOL)a_lParam;
    BOOL          bLoop    = TRUE;   
    char          szPath[MAX_PATH+1] = {0};  
    LPSTR         pszSlashPtr = szPath;  

    if(pszDBase==NULL)
        return FALSE;

    nCount = G_dbManager.GetCount();
    for(ctr=0; ctr<nCount; ctr++)
    {
        pEntry = G_dbManager[ctr];
        if(pEntry && stricmp(pEntry->m_strName, pszDBase)==0)
        {
            if(bCreate)
            {
                strncpy(szPath, pEntry->m_strFile, MAX_PATH);
                PathRemoveFileSpec(szPath);
                pszSlashPtr = strchr(pszSlashPtr, ':');
                if(pszSlashPtr!=NULL)
                    pszSlashPtr+=2;
                else
                    pszSlashPtr = szPath;
                do
                {
                    pszSlashPtr = strchr(pszSlashPtr+1, '\\');
                    if(pszSlashPtr==NULL)
                        bLoop = FALSE;

                    if(pszSlashPtr)
                        *pszSlashPtr = 0;

                    if(!PathIsDirectory(szPath))
                        if(!CreateDirectory(szPath, NULL))
                            return FALSE;
                    if(pszSlashPtr)
                        *pszSlashPtr = '\\';
                }while(bLoop);
            };
            return (G_dbManager.MountFile(ctr)!=NULL);
        }
    }

    return FALSE;
}

