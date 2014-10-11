#include "stdafx.h"
#include "LockPath.h"
#include "TTask.h"
#include "section.h"
#include "TBaseFile.h"
#include <afxtempl.h>

#define PATH_WAIT_TIME 600

class TPathEntity
{
public:
    SEGID_F  m_segId;
    long     m_nRefs;  
    CString  m_strName;
    int      m_nWaitingTasks;
    THandle  m_hSemaphore;

};

//====================================================================
class TPathEntityMap: public CMapStringToPtr
{
public:
    void         AddEntity(SEGID_F a_segID, LPCSTR a_pszName);
    TPathEntity* GetEntity(SEGID_F a_segID, LPCSTR a_pszName);
    void         ReleaseEntity(SEGID_F a_segID, LPCSTR a_pszName);
};

static TPathEntityMap    G_mapPathEntities;
static CRITICAL_SECTION  G_cs;

//***********************************************************************
static BOOL InitModule()
{
    InitializeCriticalSection(&G_cs);
    return TRUE;
};

IMPLEMENT_INIT(InitModule)

//***********************************************************************
void TPathEntityMap::AddEntity(SEGID_F a_segID, LPCSTR a_pszName)
{
    TPathEntity *pEntity = NULL;
    CString      strKey;
    
    strKey.Format("%d_%d_%s", a_segID.ndbId, a_segID.nSegId, a_pszName);
    if(Lookup(strKey, (void*&)pEntity))
    {
        pEntity->m_nRefs++;
    }
    else
    {
        pEntity = new TPathEntity;
        if(pEntity)
        {
            pEntity->m_nRefs = 1;
            pEntity->m_segId = a_segID;
            pEntity->m_strName = a_pszName;
            pEntity->m_nWaitingTasks = 0;
            SetAt(strKey, pEntity);
        }
    }
}

//***********************************************************************
TPathEntity* TPathEntityMap::GetEntity(SEGID_F a_segID, LPCSTR a_pszName)
{
    TPathEntity *pEntity = NULL;
    CString      strKey;
    
    strKey.Format("%d_%d_%s", a_segID.ndbId, a_segID.nSegId, a_pszName);
    if(Lookup(strKey, (void*&)pEntity))
    {
        return pEntity;
    }
    return NULL;
}

//***********************************************************************
void TPathEntityMap::ReleaseEntity(SEGID_F a_segID, LPCSTR a_pszName)
{
    TPathEntity *pEntity = NULL;
    CString      strKey;
    
    strKey.Format("%d_%d_%s", a_segID.ndbId, a_segID.nSegId, a_pszName);
    if(Lookup(strKey, (void*&)pEntity))
    {
        if((--pEntity->m_nRefs)==0)
        {
            delete pEntity;
            RemoveKey(strKey);
        }
    }
}

//***********************************************************************
void LOCKPATH_AddPath(LPCSTR a_pszPath, TIDArray &a_Array)
{
    CString strPath(a_pszPath), strName;
    long    ctr=0, nCount=0, nPos=0, nFind=0;
    SEGID_F segId={0,0};

    TCSLock cs(&G_cs);

    //-------------------------------------
    nPos   = strPath.Find('\\');
    nCount = a_Array.GetSize();
    if(nPos!=-1)
    {
        nPos++;
    }else
        nPos=0;
    //-------------------------------------
    for(ctr=0; ctr<nCount; ctr++)
    {
        segId = a_Array[ctr];
        nFind  = strPath.Find('\\', nPos);
        if(nFind==-1)
            break;
        strName = strPath.Mid(nPos, nFind-nPos);
        G_mapPathEntities.AddEntity(segId, strName);
        nPos = nFind+1;
    }
    //-------------------------------------
}

//***********************************************************************
void LOCKPATH_RemovePath(LPCSTR a_pszPath, TIDArray &a_Array)
{
    CString      strPath(a_pszPath), strName;
    long         ctr=0, nCount=0, nPos=0, nFind=0;
    SEGID_F      segId   = {0,0};
    TPathEntity *pEntity = NULL;
    TTask       *pTask   = NULL;  
    POSITION     pos     = NULL; 

    TCSLock cs(&G_cs);

    //-------------------------------------
    nPos   = strPath.Find('\\');
    nCount = a_Array.GetSize();
    if(nPos!=-1)
    {
        nPos++;
    }else
        nPos=0;
    //-------------------------------------
    for(ctr=0; ctr<nCount; ctr++)
    {
        segId = a_Array[ctr];
        nFind  = strPath.Find('\\', nPos);
        if(nFind==-1)
            break;
        strName = strPath.Mid(nPos, nFind-nPos);
        pEntity = G_mapPathEntities.GetEntity(segId, strName);
        if(pEntity && pEntity->m_nRefs == 1)
        {
            if(pEntity->m_nWaitingTasks)
                ReleaseSemaphore(pEntity->m_hSemaphore, pEntity->m_nWaitingTasks, NULL); // wake up all
        }
        G_mapPathEntities.ReleaseEntity(segId, strName);
        nPos = nFind+1;
    }
    //-------------------------------------
}


//***********************************************************************
BOOL LOCKPATH_WaitForEntity(SEGID_F a_segId, LPCSTR a_pszSubName)
{
    TPathEntity *pEntity = NULL;
    BOOL         bOK     = FALSE;
    //-------------------------------------

    TCSLock cs(&G_cs);

    pEntity = G_mapPathEntities.GetEntity(a_segId, a_pszSubName);
    if(pEntity == NULL)
        return TRUE;

    pEntity->m_nWaitingTasks++;
    if(pEntity->m_hSemaphore == NULL)
        pEntity->m_hSemaphore = CreateSemaphore(NULL, 0, 100, NULL);

    //-------------------------------------
    cs.unlock();
    bOK = (WaitForSingleObject(pEntity->m_hSemaphore, PATH_WAIT_TIME) == WAIT_OBJECT_0);
    cs.lock();
    //-------------------------------------
    pEntity->m_nWaitingTasks--;
    //-------------------------------------
    return bOK;
}
