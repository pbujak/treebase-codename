#include "stdafx.h"
#include <afxtempl.h>
#include "label.h"
#include "TTask.h"
#include "DataMgr.h"
#include "SegmentMgr.h"
#include "TBaseFile.h"
#include "InternalSection.h"

CRITICAL_SECTION G_cs;

typedef struct
{
    long     nSegId;
    ID_ENTRY nLabelId;
}LABEL_ENTRY;

typedef enum {eFound, eInsert, eAdd} eFindStatus;

typedef struct
{
    eFindStatus m_eStatus;
    long        m_nIndex;    
}BSEARCH_INFO;

class TLabelArray: public CArray<LABEL_ENTRY, LABEL_ENTRY&>
{
    long GetSegIdOfIndex(long a_nIndex);
    void BinarySearch   (long a_nSegId, BSEARCH_INFO *a_pInfo);
public:
    long Find(long a_nSegId);
    long Add (LABEL_ENTRY &a_rEntry);    
};

class TLabelManager: public TLabelManagerBase
{
    BOOL         m_bLoaded;
    TLabelArray  m_arrLabelIds;
    CRITICAL_SECTION m_cs;

    BOOL          LoadLabelData    (TLabelSection *a_pSection);
    long          FindLabel        (TLabelSection *a_pSection, long a_nSegSection);
    BOOL          CreateLabel      (TLabelSection *a_pSection, long a_nSegSection, LPCSTR  a_pszLabel);
    BOOL          ModifyLabel      (TLabelSection *a_pSection, long a_nSegSection, LPCSTR  a_pszLabel);
    BOOL          ReloadLabel      (TLabelSection *a_pSection, long a_nSegSection, LPCSTR a_pszLabel);

public:
    BOOL          EnsureLabels     (TLabelSection *a_pSection);
    BOOL          GetSectionLabel  (TLabelSection *a_pSection, long a_nSegSection, CString &a_strLabel);
    BOOL          SetSectionLabel  (TLabelSection *a_pSection, long a_nSegSection, LPCSTR  a_pszLabel);
    BOOL          DeleteSectionLabel(TLabelSection *a_pSection, long a_nSegSection);
    inline TLabelManager()
    {
        InitializeCriticalSection(&m_cs);
        m_bLoaded = FALSE;
    };
    ~TLabelManager();
    //------------------------------------
    inline void Lock()
    {
        EnterCriticalSection(&m_cs);
    }
    //------------------------------------
    inline void Unlock()
    {
        LeaveCriticalSection(&m_cs);
    }
    //------------------------------------
    virtual void Delete();
};

//**************************************************************
TLabelManager* _GetManager(long a_ndbId)
{
    TDBFileEntry *pEntry = G_dbManager[a_ndbId];

    if(pEntry)
    {
        return (TLabelManager*)pEntry->m_pLabelManager;
    }
    return NULL;
}

//**************************************************************
static BOOL Init()
{
    InitializeCriticalSection(&G_cs);
    return TRUE;
}

IMPLEMENT_INIT(Init)

//**************************************************************
long TLabelArray::GetSegIdOfIndex(long a_nIndex)
{
    LABEL_ENTRY *pEntry = &ElementAt(a_nIndex);
    return pEntry->nSegId;
}

//**************************************************************
void TLabelArray::BinarySearch(long a_nSegId, BSEARCH_INFO *a_pInfo)
{
    long  nSegId = -1;
    long  nCount = GetSize();
    long  nMinIndex=0, nMaxIndex=0;
    long  nDiffIndex=0, nMidIndex=0;

    //------------------------------------------------
    if (nCount==0)
    {
        a_pInfo->m_eStatus = eAdd;
        return;
    }

    nMinIndex = 0;
    nMaxIndex = nCount-1;
    //------------------------------------------------
    if (GetSegIdOfIndex(0)>a_nSegId)
    {
        a_pInfo->m_eStatus = eInsert;
        a_pInfo->m_nIndex = 0;
        return;
    }
    if (GetSegIdOfIndex(nMaxIndex)<a_nSegId)
    {
        a_pInfo->m_eStatus = eAdd;
        return;
    }
    if (GetSegIdOfIndex(0)==a_nSegId)
    {
        a_pInfo->m_eStatus = eFound;
        a_pInfo->m_nIndex = 0;
        return;
    }
    if (GetSegIdOfIndex(nMaxIndex)==a_nSegId)
    {
        a_pInfo->m_eStatus = eFound;
        a_pInfo->m_nIndex = nMaxIndex;
        return;
    }
    //------------------------------------------------
    while(1)
    {
        nDiffIndex = nMaxIndex-nMinIndex;
        if (nDiffIndex==0)
        {
            a_pInfo->m_eStatus = eInsert;
            a_pInfo->m_nIndex = nMaxIndex+1;
            return;
        }
        if (nDiffIndex==1)
        {
            a_pInfo->m_eStatus = eInsert;
            a_pInfo->m_nIndex = nMaxIndex;
            return;
        }
        nMidIndex = (nMinIndex + nMaxIndex)>>1;
        nSegId = GetSegIdOfIndex(nMidIndex);

        if (a_nSegId==nSegId)
        {
            a_pInfo->m_eStatus = eFound;
            a_pInfo->m_nIndex = nMidIndex;
            return;
        }
        if (a_nSegId<nSegId)
        {
            nMaxIndex = nMidIndex;    
        }
        if (a_nSegId>nSegId)
        {
            nMinIndex = nMidIndex;    
        }
    }
    //------------------------------------------------
};

//**************************************************************
long TLabelArray::Find(long a_nSegId)
{
    BSEARCH_INFO xInfo;
    BinarySearch(a_nSegId, &xInfo);
    if (xInfo.m_eStatus==eFound)
        return xInfo.m_nIndex;
    return -1;
}

//**************************************************************
long TLabelArray::Add(LABEL_ENTRY &a_rLabel)
{
    BSEARCH_INFO xInfo;
    BinarySearch(a_rLabel.nSegId, &xInfo);

    if (xInfo.m_eStatus==eFound)
    {
        return -1;
    }
    else if (xInfo.m_eStatus==eAdd)
    {
        return CArray<LABEL_ENTRY, LABEL_ENTRY&>::Add(a_rLabel);
    }
    else if (xInfo.m_eStatus==eInsert)
    {
        CArray<LABEL_ENTRY, LABEL_ENTRY&>::InsertAt(xInfo.m_nIndex, a_rLabel);
        return xInfo.m_nIndex;
    }
    return -1;
}

//**************************************************************
TLabelManager::~TLabelManager()
{
    DeleteCriticalSection(&m_cs);
};

//**************************************************************
TBITEM_ENTRY* TLabelSection::GetCacheLabelData(ID_ENTRY *a_pLabelId)
{
   FETCH_VALUE_STRUCT *pFetch  = NULL;
   TBITEM_ENTRY       *pEntry  = NULL;         
   long                nOffset = 0;

   if (m_pvCache==NULL)
       return NULL;

    //----------------------------------------------------------
    do
    {
        pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, m_pvCache, nOffset);
        pEntry = &pFetch->item;
        if (pEntry->dwHash==a_pLabelId->dwHash && pEntry->wOrder==a_pLabelId->wOrder)
            return pEntry;
        nOffset = pFetch->ofsNext;
    }while (nOffset!=0);
    //----------------------------------------------------------
    return NULL;
}

//**********************************************************
BOOL TLabelSection::EnsureValueByName(LPCSTR a_pszLabel)
{
    if (m_pvCache)
    {
        TASK_MemFree(m_pvCache);
        m_pvCache = NULL;
    }
    //-----------------------------------------------
    m_pvCache = FetchValueByName(a_pszLabel, FALSE);
    return (m_pvCache!=NULL);
}

//**************************************************************
TBITEM_ENTRY* TLabelSection::GetCacheLabelDataByName(LPCSTR a_pszName)
{
   FETCH_VALUE_STRUCT *pFetch  = NULL;
   TBITEM_ENTRY       *pEntry  = NULL;         
   TBVALUE_ENTRY      *pValue  = NULL;         
   long                nOffset = 0;
   LPCSTR              pszName = NULL; 

   if (m_pvCache==NULL)
       return NULL;

   //----------------------------------------------------------
   do
   {
       pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, m_pvCache, nOffset);
       pEntry = &pFetch->item;
       pszName = TBITEM_NAME(pEntry);
       if (stricmp(pszName, a_pszName)==0)
           return pEntry;
       nOffset = pFetch->ofsNext;
   }while (nOffset!=0);
   //----------------------------------------------------------
   return NULL;
}


//**************************************************************
TBITEM_ENTRY* TLabelSection::GetLabelData(ID_ENTRY *a_pLabelId)
{
    TBITEM_ENTRY    *pEntry = NULL;

    //-------------------------------------------
    pEntry = GetCacheLabelData(a_pLabelId);
    if (pEntry)
        return pEntry;

    //-------------------------------------------
    if (m_pvCache!=NULL)
    {
        TASK_MemFree(m_pvCache);
        m_pvCache = NULL;
    }
    //-------------------------------------------
    m_pvCache = FetchValueByID(a_pLabelId->dwHash, a_pLabelId->wOrder);
    if (m_pvCache)
        return GetCacheLabelData(a_pLabelId);
    return NULL;
}

//**************************************************************
void TLabelManager::Delete()
{
    delete this;
}

//**************************************************************
BOOL TLabelManager::LoadLabelData(TLabelSection *a_pSection)
{
    LABEL_ENTRY    entry  = {0};
    TBITEM_ENTRY  *pItem  = NULL;
    TBVALUE_ENTRY *pValue = NULL;
    long           nCount   = 0;
    long           ctr      = 0;
    DWORD          dwType   = 0; 

    //-------------------------------------------
    Util::Internal::TSectionIterator iterator(a_pSection);

    //-------------------------------------------
    ITERATED_ITEM_ENTRY *pCurrent = iterator.getNext();
    while(pCurrent != NULL)
    {
        entry.nLabelId = pCurrent->id;
        pItem = a_pSection->GetLabelData(&pCurrent->id);
        if (pItem)
        {
            pValue = TBITEM_VALUE(pItem);
            dwType = TBITEM_TYPE(pItem);
            if (dwType==TBVTYPE_SECTION)
            {
                entry.nSegId = pValue->int32;
                m_arrLabelIds.Add(entry);
            }
        }
        pCurrent = iterator.getNext();
    }
    //-------------------------------------------
    return TRUE;

}

//**************************************************************
long TLabelManager::FindLabel(TLabelSection *a_pSection, long a_nSegSection)
{
    long ctr=0, nCount=0;

    if (!EnsureLabels(a_pSection))
        return -1;

    return m_arrLabelIds.Find(a_nSegSection);
}

//**************************************************************
BOOL TLabelManager::GetSectionLabel(TLabelSection *a_pSection, long a_nSegSection, CString &a_strLabel)
{
    ID_ENTRY      *pLabelId = NULL;
    long           nIndex   = -1;
    TBITEM_ENTRY  *pItem    = NULL; 
    LPCSTR         pszName  = NULL; 

    nIndex = FindLabel(a_pSection, a_nSegSection);
    if (nIndex==-1)
    {
        a_strLabel.Empty();
        return TRUE;
    }

    pLabelId = &m_arrLabelIds[nIndex].nLabelId;
    pItem = a_pSection->GetLabelData(pLabelId);
    if (pItem)
    {
        pszName = TBITEM_NAME(pItem);
        a_strLabel = pszName;
    }
    return TRUE;
}

//**************************************************************
BOOL TLabelSection::CanModifyLabel()
{
    return FALSE;
};


//**************************************************************
BOOL TLabelManager::ReloadLabel(TLabelSection *a_pSection, long a_nSegSection, LPCSTR a_pszLabel)
{
    TBITEM_ENTRY *pItem = NULL;
    LABEL_ENTRY   entry  = {0};

    //-----------------------------------------------
    if (a_pSection->EnsureValueByName(a_pszLabel))
    {
        pItem = a_pSection->GetCacheLabelDataByName(a_pszLabel);
        if (pItem)
        {
            entry.nLabelId.dwHash = pItem->dwHash;
            entry.nLabelId.wOrder = pItem->wOrder;
            entry.nSegId = a_nSegSection;
            m_arrLabelIds.Add(entry);
            return TRUE;
        }
    }
    //-----------------------------------------------
    return FALSE;
}

//**************************************************************
BOOL TLabelManager::CreateLabel(TLabelSection *a_pSection, long a_nSegSection, LPCSTR a_pszLabel)
{
    if (!a_pSection->CreateLabel(a_nSegSection, a_pszLabel))
        return FALSE;
    
    return ReloadLabel(a_pSection, a_nSegSection, a_pszLabel);
}

//**************************************************************
BOOL TLabelManager::ModifyLabel(TLabelSection *a_pSection, long a_nSegSection, LPCSTR a_pszLabel)
{
    CString strOldName; 
    long    nIndex = -1;

    if (GetSectionLabel(a_pSection, a_nSegSection, strOldName))
    {
        if (strOldName.CompareNoCase(a_pszLabel)!=0)
        {
            if (a_pSection->ModifyLabel(strOldName, a_pszLabel))
            {
                nIndex = FindLabel(a_pSection, a_nSegSection);
                if (nIndex!=-1)
                {
                    m_arrLabelIds.RemoveAt(nIndex); 
                }
                return ReloadLabel(a_pSection, a_nSegSection, a_pszLabel);
            }
        }
    }
    return FALSE;
}

//**************************************************************
BOOL TLabelManager::SetSectionLabel(TLabelSection *a_pSection, long a_nSegSection, LPCSTR a_pszLabel)
{
    long  nIndex   = -1;
    //-----------------------------------------------
    if (!m_bLoaded)
        m_bLoaded = LoadLabelData(a_pSection);
    if (!m_bLoaded)
        return FALSE;
    //-----------------------------------------------
    nIndex = FindLabel(a_pSection, a_nSegSection);
    if (nIndex==-1)
    {
        return CreateLabel(a_pSection, a_nSegSection, a_pszLabel);
    }
    else
        return ModifyLabel(a_pSection, a_nSegSection, a_pszLabel);
}

//**************************************************************
BOOL TLabelManager::DeleteSectionLabel(TLabelSection *a_pSection, long a_nSegSection)
{
    ID_ENTRY      *pLabelId = NULL;
    long           nIndex   = -1;
    TBITEM_ENTRY  *pItem    = NULL; 
    LPCSTR         pszName  = NULL; 

    //-----------------------------------------------
    if (!EnsureLabels(a_pSection))
        return FALSE;
    //-----------------------------------------------
    nIndex = FindLabel(a_pSection, a_nSegSection);
    if (nIndex==-1)
    {
        return TRUE;
    }

    pLabelId = &m_arrLabelIds[nIndex].nLabelId;
    pItem = a_pSection->GetLabelData(pLabelId);
    //-----------------------------------------------
    if (pItem)
    {
        pszName = TBITEM_NAME(pItem);
        if (a_pSection->DeleteLabel(pszName))
        {
            m_arrLabelIds.RemoveAt(nIndex);
        };
    }
    return TRUE;
}

//**************************************************************
BOOL TLabelManager::EnsureLabels(TLabelSection *a_pSection)
{
    if (!m_bLoaded)
        m_bLoaded = LoadLabelData(a_pSection);
    return m_bLoaded;
};

//**************************************************************
TLabelSection::TLabelSection(long a_ndbId)
{
    m_bSupportLabel = FALSE;
    m_nSegId = SEGMENT_GetLabelSection(m_ndbId);
    m_arrPathIDs.M_Add(m_ndbId, m_nSegId);
    m_strName = "~";
    m_pvCache = NULL;
};

//**************************************************************
TLabelSection::~TLabelSection()
{
    if (m_pvCache==NULL)
    {
        TASK_MemFree(m_pvCache);
        m_pvCache = NULL;
    }
};


//****************************************************************************
BOOL TLabelSection::CreateLabel(long a_nSegId, LPCSTR a_pszLabel)
{
    TSecCreateExtra xParams;
    TSection        xSection;
    long            nSegId = 0;
    BOOL            bRetVal = FALSE; 
    TTask          *pTask   = NULL; 

    nSegId = FindSection(a_pszLabel);
    if (nSegId==0)
    {
        TASK_ClearErrorInfo();
        xParams.nSegId = a_nSegId;
        xParams.bAddRef = FALSE;
        bRetVal = TSection::CreateSection(a_pszLabel, &xSection, 0, NULL/*AllAccessSD*/, &xParams);
        if (!bRetVal)
        {
            TASK_SetErrorInfo(TRERR_CANNOT_SET_LABEL, NULL, NULL);
        }
    }
    else
    {
        TASK_SetErrorInfo(TRERR_LABEL_ALREADY_USED, NULL, NULL);
    }
    return bRetVal;
}

//**********************************************************
BOOL TLabelSection::ModifyLabel(LPCSTR a_pszOldLabel, LPCSTR a_pszNewLabel)
{
    long nSegId = FindSection(a_pszNewLabel);

    if (nSegId==0)
    {
        if (!TSection::RenameSection(a_pszOldLabel, a_pszNewLabel))
        {
            TASK_SetErrorInfo(TRERR_CANNOT_SET_LABEL, NULL, NULL);
            return FALSE;
        };
    }
    else
    {
        TASK_SetErrorInfo(TRERR_LABEL_ALREADY_USED, NULL, NULL);
        return FALSE;
    }
    return TRUE;
}

//*********************************************************
BOOL TLabelSection::DeleteLabel(LPCSTR a_pszLabel)
{
    long     nSegId = 0; 
    long     nRefCount = 0;
    BOOL     bRetVal= FALSE;
    //------------------------------------------------
    if (Open())
    {
        if(Lock())
        {
            bRetVal = TRUE;
            nSegId = FindSection(a_pszLabel);
            if (nSegId!=0)
                bRetVal = DeleteSegmentItem(a_pszLabel, nSegId, FALSE);
            Unlock();
        }
        Close();
    }
    //------------------------------------------------
    return bRetVal;
}

//*********************************************************
BOOL LABEL_EnsureLabels(long a_ndbId)
{
    TLabelSection  *pLabelSection = TASK_GetLabelSection(a_ndbId);
    TLabelManager  *pLabelManager = _GetManager(a_ndbId); 
    BOOL            bRetVal       = FALSE;  

    if(pLabelManager)
    {
        pLabelManager->Lock();
        bRetVal = pLabelManager->EnsureLabels(pLabelSection);
        pLabelManager->Unlock();
    }
    return bRetVal;
}

//*********************************************************
BOOL LABEL_GetSectionLabel(TSection *a_pSection, CString &a_strLabel)
{
    long              ndbId = a_pSection ? a_pSection->m_ndbId : 0;
    TTrans            xTrans(ndbId);
    TLabelSection    *pLabelSection = TASK_GetLabelSection(ndbId);
    TLabelManager    *pManager = NULL;
    BOOL              bRetVal = FALSE;

    if (!a_pSection->m_bSupportLabel)
    {
       return TRUE;
    }
    pManager = _GetManager(a_pSection->m_ndbId);

    if (pManager && a_pSection->Open())
    {
        pManager->Lock();
        bRetVal = pManager->GetSectionLabel(pLabelSection, a_pSection->m_nSegId, a_strLabel);
        pManager->Unlock();
        if (!bRetVal)
        {
            TASK_SetErrorInfo(0, NULL, a_pSection->m_strName);
        }
        a_pSection->Close();
    }
    return bRetVal;
}

//*********************************************************
BOOL LABEL_SetSectionLabel(TSection *a_pSection, LPCSTR a_pszLabel)
{
    long              ndbId = a_pSection ? a_pSection->m_ndbId : 0;
    TTrans            xTrans(ndbId);
    TLabelSection    *pLabelSection = TASK_GetLabelSection(ndbId);
    BOOL              bRetVal = FALSE;
    TLabelManager    *pManager = _GetManager(ndbId);  

    if(!pManager)
        return FALSE;

    if (strlen(a_pszLabel)==0)
        return LABEL_DeleteSectionLabel(a_pSection);

    if (!a_pSection->m_bSupportLabel)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, NULL, a_pSection->m_strName);
        return FALSE;
    }
    if (!a_pSection->CanModifyLabel())
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, a_pSection->m_strName);
        return FALSE;
    }
    if (a_pSection->Open())
    {
        if (a_pSection->Lock())
        {
            pManager->Lock();
            bRetVal = pManager->SetSectionLabel(pLabelSection, a_pSection->m_nSegId, a_pszLabel);
            pManager->Unlock();
            if (!bRetVal)
            {
                TASK_SetErrorInfo(0, NULL, a_pSection->m_strName);
            }
            a_pSection->Unlock();
        }
        a_pSection->Close();
    }
    return bRetVal;
}

//*********************************************************
BOOL LABEL_DeleteSectionLabel(TSection *a_pSection)
{
    long              ndbId = a_pSection ? a_pSection->m_ndbId : 0;
    TTrans            xTrans(ndbId);
    TLabelManager    *pManager = _GetManager(ndbId);  
    TLabelSection    *pLabelSection = TASK_GetLabelSection(ndbId);
    BOOL              bRetVal = FALSE;

    if(!pManager)
        return FALSE;

    if (!a_pSection->m_bSupportLabel)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, NULL, a_pSection->m_strName);
        return FALSE;
    }
    if (!a_pSection->CanModifyLabel())
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, a_pSection->m_strName);
        return FALSE;
    }
    if (a_pSection->Open())
    {
        if (a_pSection->Lock())
        {
            pManager->Lock();
            bRetVal = pManager->DeleteSectionLabel(pLabelSection, a_pSection->m_nSegId);
            pManager->Unlock();
            if (!bRetVal)
            {
                TASK_SetErrorInfo(0, NULL, a_pSection->m_strName);
            }
            a_pSection->Unlock();
        }
        a_pSection->Close();
    }
    return bRetVal;
}

//*********************************************************
BOOL LABEL_DeleteSectionSegmentLabel(long a_ndbId, long a_nSegSection)
{
    TTrans            xTrans(a_ndbId);
    TLabelManager    *pManager = _GetManager(a_ndbId);  
    TLabelSection    *pLabelSection = TASK_GetLabelSection(a_ndbId);
    BOOL              bRetVal = FALSE;  

    if(pManager)
    {
        pManager->Lock();
        bRetVal = pManager->DeleteSectionLabel(pLabelSection, a_nSegSection);
        pManager->Unlock();
    }
    return bRetVal;
}

//*********************************************************
TLabelManagerBase *LABEL_CreateManager()
{
    return new TLabelManager();
}
