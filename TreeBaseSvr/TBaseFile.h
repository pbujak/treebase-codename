// TBaseFile.h: interface for the TBaseFile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(TBASEFILE_H)
#define TBASEFILE_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <afxtempl.h>
#include "TreeBaseInt.h"
#include "TDataFile.h"
#include "Storage.h"

#define PAGECOUNT 512

#define SEGID_RESERVED 0x00FFFFFF
#define SEGID_TABLE    0x00FFFF01
#define SEGID_SEGMENTDIR 0x00FFFF02

class TSegment;
class TLabelManagerBase;

typedef unsigned __int64 QWORD;


#define SEGID_IS_NULL(x) (x.ndbId==0 && x.nSegId==0)

extern int FILE_dbIdTemp;

//************************************************************

//============================================================
void FILE_Lock();
void FILE_Unlock();
int  FILE_MountDatabase(SEGID_F a_segMntPoint, LPCSTR a_pszName, LPCSTR a_pszFile);
BOOL FILE_InitTempDatabase();

struct TDBFileEntry
{
    long               m_ndbID; 
    SEGID_F            m_segMntPoint; 
    CString            m_strFile; 
    CString            m_strName;
    CString            m_strStatusKey;
    Storage::TStorage *m_pStorage;
    TLabelManagerBase *m_pLabelManager;  
    long               m_nRefs; 
    RWLOCK             m_rwLock;
    BOOL               m_bCheckMedia;
    DWORD              m_dwLastAccessTime;
};

template<> UINT AFXAPI HashKey<SEGID_F&>(SEGID_F& a_rSegIdf);

//===========================================================
inline BOOL operator==(SEGID_F a_rSegId1, SEGID_F a_rSegId2)
{
    return (a_rSegId1.nSegId==a_rSegId2.nSegId) && (a_rSegId1.ndbId==a_rSegId2.ndbId);
}

class TDBManager
{
    TCriticalSection m_cs;
    
    int   m_nSharedLockWaiters;
    int   m_nExclusiveLockWaiters;
    int   m_nSharedLockCount;
    bool  m_bExclusiveLock;

    TGenericHandle<HANDLE>               m_hSemLockShared, m_hSemLockExclusive;
    CArray<TDBFileEntry*, TDBFileEntry*> m_arrEntries;
    CMap<SEGID_F, SEGID_F&, long, long>  m_mapBySegID;
public:
    inline TDBManager(): 
        m_nSharedLockCount(0), 
        m_bExclusiveLock(false),
        m_nSharedLockWaiters(0),
        m_nExclusiveLockWaiters(0)
    {
        m_hSemLockShared    = CreateSemaphore(NULL, 0, 10, NULL);
        m_hSemLockExclusive = CreateSemaphore(NULL, 0, 10, NULL);
    }
    inline ~TDBManager()
    {
    }

    Storage::TStorage* MountFile (long a_nID);
    BOOL               UmountFile(long a_nID);
	Storage::TStorage* GetFile   (long a_nID);
    BOOL               IncFileRefs(long a_nID);
    BOOL               DecFileRefs(long a_nID);
    long               AddEntry  (SEGID_F a_segMntPoint, LPCSTR a_pszName, LPCSTR a_pszFile);
    TDBFileEntry* operator[](long a_nID);
    TDBFileEntry* operator[](SEGID_F &a_segMntPoint);

    inline long GetCount()
    {
        TCSLock _lock(&m_cs);
        return m_arrEntries.GetSize();
    }
    void LockShared();
    void LockExclusive();

    void UnlockShared();
    void UnlockExclusive();
};

extern TDBManager G_dbManager;
extern SEGID_F    SEGID_NULL;

#endif // !defined(TBASEFILE_H)
