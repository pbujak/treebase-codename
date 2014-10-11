#include "stdafx.h"
#include "tbcore.h"
#include "TreeBaseInt.h"
#include "lpccodes.h"
#include "lpc.h"
#include <malloc.h>
#include <vector>
#include "value.h"
#include "blob.h"
#include <shlwapi.h>
#include <atlsecurity.h>
#include "Shared\TAbstractThreadLocal.h"

extern TThreadLocal<TTaskInfo*> G_pTask;

typedef std::set<TTaskInfo*>           TTaskSet;
typedef std::set<TTaskInfo*>::iterator TTaskSetIterator;

TDllMemAlloc     G_malloc; 

static TClientErrorInfo S_errorInfo;
static BOOL             S_bErrorInfo = FALSE; 
static TCriticalSection S_cs;
static TTaskSet         S_setTasks;

HANDLE G_hReady = NULL;
HANDLE G_hGlobalMutex = NULL; 
HANDLE G_hModule = NULL;


class TMutexGuard
{
    HANDLE &m_hMutex;
public:
    inline TMutexGuard(HANDLE &a_hMutex): m_hMutex(a_hMutex)
    {
        WaitForSingleObject(m_hMutex, INFINITE);
    }

    inline ~TMutexGuard()
    {
        ReleaseMutex(m_hMutex);
    }
};



//************************************************************************
TString& TString::operator=(LPCSTR a_pszText)
{
    if (a_pszText)
    {
        TStringBase::operator=(a_pszText);
    }
    else
        TStringBase::operator=("");
    return *this;
};

//************************************************************************
LPVOID TDllMemAlloc::Alloc(DWORD a_cbSize)
{
    return malloc(a_cbSize);
};

//************************************************************************
LPVOID TDllMemAlloc::ReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return realloc(a_pvData, a_cbSize);
};

//************************************************************************
void TDllMemAlloc::Free(LPVOID a_pvData)
{
    free(a_pvData);
};

//************************************************************************
DWORD TDllMemAlloc::MemSize(LPVOID a_pvData)
{
    return _msize(a_pvData);
};

//**********************************************************************
void TASK_RegisterTask(TTaskInfo *a_pTask)
{
    S_cs.Enter();
    S_setTasks.insert(a_pTask);
    S_cs.Leave();
}

//**********************************************************************
void TASK_UnregisterTask(TTaskInfo *a_pTask)
{
    S_cs.Enter();
    S_setTasks.erase(a_pTask);
    S_cs.Leave();
}

//**********************************************************************
void TASK_DeleteAll()
{
    TTaskSetIterator  iter;
    TTaskInfo        *pTask = NULL; 

    S_cs.Enter();
    for(iter=S_setTasks.begin(); iter!=S_setTasks.end(); iter++)
    {
        pTask = *iter;
        if(pTask)
            delete pTask;
    };
    S_cs.Leave();
}

//**********************************************************************
TTaskInfo* TASK_GetCurrent(BOOL a_bResetState)
{
    if(!G_pTask.IsValid())
        return NULL;

    TTaskInfo *pTask = G_pTask;

    if (!S_bErrorInfo)
    {
        COMMON_Initialize(&G_malloc, &S_errorInfo);
        S_bErrorInfo = TRUE;
    }

    if (pTask != NULL)
    {
        if(a_bResetState && pTask->m_pTaskState)
        {
            delete pTask->m_pTaskState;
            pTask->m_pTaskState = NULL;
        }

        return pTask;
    }

    WaitForSingleObject(G_hReady, INFINITE);
    if(G_hGlobalMutex == NULL)
        return NULL;

    pTask = new TTaskInfo();
    if (!pTask->Init())
    {
        delete pTask;
        return NULL;
    }
    G_pTask = pTask;
    TASK_RegisterTask(pTask);
    return pTask;
}

//**********************************************************************
TSection* SECTION_GetSection(HTBSECTION a_hSection)
{
    TTaskInfo *pTaskInfo = TASK_GetCurrent();

    if (pTaskInfo==NULL)
        return NULL;

    return pTaskInfo->SectionFromHandle(a_hSection);
}

//**********************************************************************
void SECTION_DeleteFromCache(HTBSECTION a_hSection, LPCSTR a_pszName)
{
    TSection *pSection = SECTION_GetSection(a_hSection);

    if (pSection)
    {
        pSection->m_valCache.RemoveValue(a_pszName);
        pSection->FreeByIDCache();
    }
}

//**********************************************************************
void TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)
{
    TTaskInfo *pTask = TASK_GetCurrent();
    if (pTask)
    {
        if (a_dwError!=0)
            pTask->m_dwError = a_dwError;
        if (a_pszErrorItem)
            pTask->m_strErrorItem = a_pszErrorItem;
        if (a_pszSection)
            pTask->m_strErrorSection = a_pszSection;
    }
}

//**********************************************************************
void TClientErrorInfo::SetTaskErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)
{
    TASK_SetErrorInfo(a_dwError, a_pszErrorItem, a_pszSection);
};

//**********************************************************************
TTaskInfo::~TTaskInfo()
{
    TSectionMapIter iterSec;
    TPtrIter        iterVal;

    if(m_pTaskState)
        delete m_pTaskState;

    if(m_pPipe)
        m_pPipe->Close();

    if (m_hBlobBuf)
        CloseHandle(m_hBlobBuf);

    for (iterSec=m_mapSections.begin(); iterSec!=m_mapSections.end(); iterSec++)
    {
        delete (*iterSec).second;
    }
    for (iterVal=m_setPointers.begin(); iterVal!=m_setPointers.end(); iterVal++)
    {
        delete (*iterVal);
    }
}

//**********************************************************************
TPipeFactory* TTaskInfo::GetPipeFactory()
{
    ATL::CSid sid;
    {
        HANDLE hToken = NULL;
        if(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken))
            OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);
        
        ATL::CAccessToken token;
        token.Attach(hToken);
        token.GetUser(&sid);
    }

    TMutexGuard guard(G_hGlobalMutex);

    DWORD dwThreadID = GetCurrentThreadId();
    BOOL  bResult    = FALSE;
    //------------------------------------------------------------------------
    {
        TServerEntry entry;
     
        if(!entry.Open())
            return NULL;

        if(entry.Lock())
        {
            SERVER_ENTRY_BLOCK *pBlock = entry.GetBlock();
            if(pBlock)
            {
                int sidLength = sid.GetLength();

                CopySid(SECURITY_MAX_SID_SIZE, (PSID)pBlock->clientSid, (PSID)sid.GetPSID());

                pBlock->dwThreadId = dwThreadID;
                SetEvent(entry.m_hInitEvent);
                if(WaitForSingleObject(entry.m_hReplyEvent, INFINITE)==WAIT_OBJECT_0)
                {
                    bResult = pBlock->bResult;
                }
                DWORD dwErr = GetLastError();
                entry.ReleaseBlock();
            }
            entry.Unlock();
        }
    }
    //------------------------------------------------------------------------
    if(!bResult)
    {
        return NULL;
    }
    return TPipeFactory::GetInstanceForSharedMemory(dwThreadID, sid);
}

//**********************************************************************
BOOL TTaskInfo::Init()
{
    std::auto_ptr<TPipeFactory> ppf(GetPipeFactory());

    if(ppf.get() == NULL)
    {
        return FALSE;
    }

    m_pPipe = ppf->OpenAtClient();
    if (m_pPipe == NULL)
        return FALSE;

    if(!m_pPipe->SyncBarrier())
        return FALSE;

    m_hBlobBuf = m_pPipe->CreateSharedMemoryForBlob(false);

    if (m_hBlobBuf == NULL)
    {
        return FALSE;
    }

    AddNewSection(TBSECTION_ROOT, NULL, 0, TRUE);

    //------------------------------------------------------------
    // Get client info
    char szPath[MAX_PATH] = {0};
    char szUser[MAX_PATH] = {0};
    char szComputer[MAX_PATH] = {0};

    GetModuleFileName(GetModuleHandle(NULL), szPath, sizeof(szPath));
    DWORD dwSize = sizeof(szUser);
    GetUserName(szUser, &dwSize);
    dwSize = sizeof(szComputer);
    GetComputerName(szComputer, &dwSize);
    LPCSTR pszFileName = strrchr(szPath, '\\') + 1;

    //-----------------------------------------------------------
    // Send client info
    TCallParams inParams(SVP_SET_CLIENT_INFO);
    TCallParams outParams;

    inParams.SetText(SVP_CLIENT_EXE, pszFileName);
    inParams.SetText(SVP_CLIENT_USER, szUser);
    inParams.SetText(SVP_CLIENT_COMPUTER, szComputer);
    inParams.SetLong(SVP_CLIENT_THREAD, GetCurrentThreadId());

    return CallServer(inParams, outParams);
};

//#include "acl.h"
//#include <assert.h>
//***************************************************
void _xxx_test()
{
/*
    HTBACL hAcl = TBASE_AclCreate();

    BOOL bOK = true;
    bOK &= TBASE_AclSetAccessRights(hAcl, (PSID)ATL::CSid("user").GetPSID(), TBACCESS_READ);
    bOK &= TBASE_AclSetAccessRights(hAcl, (PSID)ATL::CSid("admin").GetPSID(), TBACCESS_ALL);
    bOK &= TBASE_AclSetAccessRights(hAcl, (PSID)ATL::CSid("NT AUTHORITY\\LocalService").GetPSID(), TBACCESS_BROWSE);
    bOK &= TBASE_AclSetAccessRights(hAcl, (PSID)ATL::CSid("NT AUTHORITY\\NetworkService").GetPSID(), TBACCESS_BROWSE);

    TAccessControlList *pAcl = TAccessControlList::FromHandle(hAcl);

    void *pvBlk = pAcl->GetBlock();

    HTBACL hAcl2 = (HTBACL)(new TAccessControlList(pvBlk));

    int nCount = TBASE_AclGetCount(hAcl);
    assert( nCount == TBASE_AclGetCount(hAcl2) );

    for(int i = 0; i < nCount; ++i)
    {
        DWORD dwAccessRights;
        PSID psid;
        bOK &= TBASE_AclGetEntry(hAcl2, i, &psid, &dwAccessRights);
        ATL::CSid sid((SID*)psid);
        CString account = sid.AccountName();
        CString domain = sid.Domain();
    }
*/
}

//***************************************************
BOOL TTaskInfo::CallServer(TCallParams &a_inParams, TCallParams &a_outParams)
{
    LPCSTR pszText = NULL;

    if (m_bPipeClose)
        return FALSE;

    try{
        a_inParams.Write(m_pPipe);
        m_pPipe->Commit();
        a_outParams.Read(m_pPipe);
    }
    catch (TPipeException*)
    {
        m_bPipeClose = TRUE;

        m_dwError = TRERR_BUS_ERROR;
        return FALSE;
    }

    //----------------------------------------------
    m_dwError = (DWORD)a_outParams.GetLong(SVP_SYS_ERRORCODE);

    pszText = a_outParams.GetText(SVP_SYS_ERRORITEM);
    m_strErrorItem = pszText;

    pszText = a_outParams.GetText(SVP_SYS_ERRORSECTION);
    m_strErrorSection = pszText;
    //----------------------------------------------

    return (BOOL)a_outParams.GetLong(SVP_SYS_RETURNVALUE);
}

//***************************************************
TSection* TTaskInfo::AddNewSection(HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwAttrs, BOOL a_bReadOnly)
{
    TSection *pSection = TSection::Create(a_hSection, a_pszName, a_dwAttrs);

    if (pSection)
    {
        pSection->m_bReadOnly = a_bReadOnly;
        m_mapSections[a_hSection] = pSection;
    }
    return pSection;
};


//***************************************************
TSection* TTaskInfo::SectionFromHandle(HTBSECTION a_hSection)
{
    TSectionMapIter iter = m_mapSections.find(a_hSection);
    
    if (iter==m_mapSections.end())
        return NULL;

    return (*iter).second;
}

//***************************************************
void TTaskInfo::RemoveSection(HTBSECTION a_hSection)
{
    TSectionMapIter iter     = m_mapSections.find(a_hSection);
    TSection       *pSection = NULL; 
    
    if (iter!=m_mapSections.end())
    {
        pSection = (*iter).second;
        m_mapSections.erase(iter);
        delete pSection;
    }
}

//***************************************************
void TTaskInfo::FillErrorInfo(TBASE_ERROR_INFO *a_pErrorInfo)
{
    a_pErrorInfo->dwErrorCode = m_dwError;
    strncpy(a_pErrorInfo->szErrorItem,    m_strErrorItem.c_str(),    127);
    strncpy(a_pErrorInfo->szErrorSection, m_strErrorSection.c_str(), 511);

    switch (m_dwError)
    {
        case TRERR_INVALID_NAME:
            strcpy(a_pErrorInfo->szDescription, "Invalid name");
            break;
        case TRERR_ITEM_NOT_FOUND:
            strcpy(a_pErrorInfo->szDescription, "Item not found");
            break;
        case TRERR_PATH_NOT_FOUND:
            strcpy(a_pErrorInfo->szDescription, "Path not found");
            break;
        case TRERR_ITEM_ALREADY_EXIST:
        case TRERR_ALREADY_EXIST:
            strcpy(a_pErrorInfo->szDescription, "Item already exists");
            break;
        case TRERR_ACCESS_DENIED:
            strcpy(a_pErrorInfo->szDescription, "Access denied");
            break;
        case TRERR_SHARING_DENIED:
            strcpy(a_pErrorInfo->szDescription, "Sharing denied");
            break;
        case TRERR_ILLEGAL_OPERATION:
            strcpy(a_pErrorInfo->szDescription, "Illegal operation");
            break;
        case TRERR_CANNOT_CREATE_SECTION:
            strcpy(a_pErrorInfo->szDescription, "Cannot create section");
            break;
        case TRERR_SECTION_NOT_EMPTY:
            strcpy(a_pErrorInfo->szDescription, "Section not empty");
            break;
        case TRERR_INVALID_PARAMETER :
            strcpy(a_pErrorInfo->szDescription, "Invalid parameter");
            break;
        case TRERR_OUT_OF_DATA :
            strcpy(a_pErrorInfo->szDescription, "Out of data");
            break;
        case TRERR_DISK_FULL :
            strcpy(a_pErrorInfo->szDescription, "Disk full");
            break;
        case TRERR_CANNOT_CREATE_BLOB :
            strcpy(a_pErrorInfo->szDescription, "Cannot create BLOB");
            break;
        case TRERR_INVALID_TYPE :
            strcpy(a_pErrorInfo->szDescription, "Invalid type");
            break;
        case TRERR_INVALID_DATA :
            strcpy(a_pErrorInfo->szDescription, "Invalid data");
            break;
        case TRERR_INVALID_SECURITY:
            strcpy(a_pErrorInfo->szDescription, "Invalid security");
            break;
        case TRERR_INVALID_USERID:
            strcpy(a_pErrorInfo->szDescription, "Invalid user ID");
            break;
        case TRERR_BUS_ERROR :
            strcpy(a_pErrorInfo->szDescription, "Bus error");
            break;
        case TRERR_LABEL_ALREADY_USED:
            strcpy(a_pErrorInfo->szDescription, "Label already used");
            break;
        case TRERR_CANNOT_SET_LABEL:
            strcpy(a_pErrorInfo->szDescription, "Cannot set label");
            break;
        case TRERR_SECTION_NOT_MOUNTED:
            strcpy(a_pErrorInfo->szDescription, "Section not mounted");
            break;
        case TRERR_STORAGE_FAILURE:
            strcpy(a_pErrorInfo->szDescription, "Storage failure");
            break;
        case TRERR_SERVICE_UNAVAILABLE:
            strcpy(a_pErrorInfo->szDescription, "Service unavailable");
            break;
        case TRERR_CANNOT_REGISTER_USER:
            strcpy(a_pErrorInfo->szDescription, "Cannot register user");
            break;
        case TRERR_SYSTEM_ALARM:
            strcpy(a_pErrorInfo->szDescription, "System alarm");
            break;
    }
}

//***************************************************
TSection* TSection::Create(HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwAttrs)
{
    TSection *pSection;

    if(a_hSection == TBSECTION_ROOT)
    {
        pSection = new TNoValueSection(a_hSection, "\\");
    }
    else if(a_dwAttrs & TBATTR_NOVALUES)
    {
        pSection = new TNoValueSection(a_hSection, a_pszName);
    }
    else
        pSection = new TSection(a_hSection, a_pszName);

    pSection->m_dwAttrs = a_dwAttrs;

    return pSection;
}

//***************************************************
TSection::TSection(HTBSECTION a_hSection, LPCSTR a_pszName): 
    m_hSection(a_hSection), 
    m_bCanGetValue(FALSE),
    m_pIterBlock(NULL), 
    m_pIterCurrent(NULL), 
    m_pByID(NULL), 
    m_bEditMode(FALSE), 
    m_pvNewValuesEnum(NULL)
{
    if (a_pszName!=NULL)
        m_strName = a_pszName;
}

//***************************************************
TSection::~TSection()
{
    if (m_pIterBlock != NULL)
    {
        free(m_pIterBlock);
    }
    if (m_pByID!=NULL)
        free(m_pByID);

    if (m_pvNewValuesEnum!=NULL)
        free(m_pvNewValuesEnum);
}

//***************************************************
ITERATED_ITEM_ENTRY* TSection::InternalGetFirstItems(TBASE_ITEM_FILTER *a_pFilter)
{
    TCallParams          inParams(SVF_GET_FIRST_ITEMS);
    TCallParams          outParams;
    TTaskInfo           *pTask   = TASK_GetCurrent();
    ITERATED_ITEM_ENTRY *pItems  = NULL;
    long                 cbSize  = 0; 

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)m_hSection);

    if (a_pFilter)
        inParams.SetBufferPointer(SVP_FILTER, a_pFilter, sizeof(TBASE_ITEM_FILTER));

    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    if (!outParams.ExtractBuffer(SVP_ITEMS, (LPVOID*)&pItems, cbSize))
        return NULL;
    return pItems;
}

//***************************************************
ITERATED_ITEM_ENTRY* TSection::InternalGetNextItems()
{
    TCallParams          inParams(SVF_GET_NEXT_ITEMS);
    TCallParams          outParams;
    TTaskInfo           *pTask   = TASK_GetCurrent();
    ITERATED_ITEM_ENTRY *pItems  = NULL;
    long                 cbSize  = 0; 

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)m_hSection);

    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    if (!outParams.ExtractBuffer(SVP_ITEMS, (LPVOID*)&pItems, cbSize))
        return NULL;
    return pItems;
}

//***************************************************
FETCH_VALUE_STRUCT* TSection::FetchValueByID(DWORD a_dwHash, WORD a_wOrder)
{
    TCallParams          inParams(SVF_FETCH_VALUE_BY_ID);
    TCallParams          outParams;
    FETCH_VALUE_STRUCT*  pFetch = NULL;
    TTaskInfo           *pTask   = TASK_GetCurrent();
    long                 cbSize  = 0; 

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)m_hSection);
    inParams.SetLong  (SVP_HASH,     (long)a_dwHash);
    inParams.SetLong  (SVP_ORDER,    (long)a_wOrder);

    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    if (!outParams.ExtractBuffer(SVP_FETCH_VALUE, (LPVOID*)&pFetch, cbSize))
        return NULL;
    return pFetch;
}

//***************************************************
TBITEM_ENTRY *TSection::FindInCacheById(DWORD a_dwHash, WORD a_wOrder)
{
    long                nOffset = 0;
    FETCH_VALUE_STRUCT *pFetch  = NULL;
    TBITEM_ENTRY       *pEntry  = NULL;

    if (m_pByID==NULL)
        return NULL;

    //----------------------------------------------------------
    do
    {
        pFetch = MAKE_PTR(FETCH_VALUE_STRUCT*, m_pByID, nOffset);
        pEntry = &pFetch->item;
        if (pEntry->dwHash==a_dwHash && pEntry->wOrder==a_wOrder)
            return pEntry;
        nOffset = pFetch->ofsNext;
    }while (nOffset!=0);
    //----------------------------------------------------------
    return NULL;
}

//***************************************************
FETCH_VALUE_STRUCT* TSection::FetchValueByName(LPCSTR a_pszName)
{
    TCallParams          inParams(SVF_FETCH_VALUE_BY_NAME);
    TCallParams          outParams;
    FETCH_VALUE_STRUCT*  pFetch = NULL;
    TTaskInfo           *pTask   = TASK_GetCurrent();
    long                 cbSize  = 0; 

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle      (SVP_HSECTION, (HTBHANDLE)m_hSection);
    inParams.SetTextPointer (SVP_NAME,     a_pszName);

    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    if (!outParams.ExtractBuffer(SVP_FETCH_VALUE, (LPVOID*)&pFetch, cbSize))
        return NULL;
    return pFetch;
}


//***************************************************
BOOL TSection::GetNextItem(LPSTR a_pszBuffer, DWORD *a_pdwType)
{
    BOOL            bFound = FALSE;  
    DWORD           dwHash = 0;
    DWORD           dwType = 0;
    WORD            wOrder = 0;
    TBITEM_ENTRY    *pEntry    = NULL;
    TBITEM_ENTRY    *pEntryNew = NULL;
    TB_BATCH_ENTRY  *pBatch    = NULL;
    LPCSTR          pszName    = NULL;
    //----------------------------------------------------------
    if (IsBadWritePtr(a_pszBuffer, 128) || IsBadWritePtr(a_pdwType, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return NULL;
    }
    //----------------------------------------------------------
    // Najpierw przeszukuj niezapisane pozycje
    if (m_pvNewValuesEnum)
        while(m_nNewValuesPos != -1)
        {
            pBatch = MAKE_PTR(TB_BATCH_ENTRY*, m_pvNewValuesEnum, m_nNewValuesPos);
            m_nNewValuesPos = pBatch->ofsNext;
            if (m_nNewValuesPos == 0)
                m_nNewValuesPos = -1;
            if (pBatch->action != eActionDel)
            {
                pszName = TBITEM_NAME(&pBatch->item);
                dwType  = TBITEM_TYPE(&pBatch->item);
                if (m_filter.IsSuitable(pszName, dwType))
                    if (m_valForUpdate.FindValue(pszName))
                    {
                        strcpy(a_pszBuffer, pszName);
                        *a_pdwType = TBITEM_TYPE(&pBatch->item);
                        return TRUE;
                    }
            }
        }
    //----------------------------------------------------------
    if (m_pIterBlock == NULL)
        return FALSE;

    //----------------------------------------------------------
    while(m_pIterCurrent->wSize != ITERATOR_END_ALL)
    {
        if (m_pIterCurrent->wSize == ITERATOR_END_BLOCK)
        {
            free(m_pIterBlock);
            m_pIterBlock = InternalGetNextItems();

            if(m_pIterBlock == NULL || m_pIterBlock->wSize == ITERATOR_END_ALL)
                return FALSE;

            m_pIterCurrent = m_pIterBlock;
        }

        //-----------------------------------------------------
        ITERATED_ITEM_ENTRY *pResult = m_pIterCurrent;
        m_pIterCurrent = MAKE_PTR(ITERATED_ITEM_ENTRY*, m_pIterCurrent, m_pIterCurrent->wSize);

        if (m_valForUpdate.FindValue(pResult->szName) == NULL)
        {
            strcpy(a_pszBuffer, pResult->szName);
            *a_pdwType = pResult->dwType;
            return TRUE;
        }
    }
    //----------------------------------------------------------
    free(m_pIterBlock);
    m_pIterBlock = m_pIterCurrent = NULL;

    return FALSE;
}

//***************************************************
BOOL TSection::GetFirstItem(TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType)
{
    if (IsBadWritePtr(a_pszBuffer, 128) || IsBadWritePtr(a_pdwType, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return NULL;
    }
    //----------------------------------------------------------
    if (m_pIterBlock != NULL)
    {
        free(m_pIterBlock);
        m_pIterBlock = NULL;
    }
    m_pIterCurrent = 0;
    //----------------------------------------------------------
    if (m_pvNewValuesEnum!=NULL)
    {
        free(m_pvNewValuesEnum);
        m_pvNewValuesEnum = FALSE;
    }
    m_nNewValuesPos = 0;
    //----------------------------------------------------------
    if (m_valForUpdate.GetCount() > 0)
    {
        m_pvNewValuesEnum = m_valForUpdate.ExportBatch();
    }

    m_pIterBlock = InternalGetFirstItems(a_pFilter);
    if (m_pIterBlock == NULL)
        return FALSE;

    m_pIterCurrent = m_pIterBlock;

    m_filter.PrepareFilter(a_pFilter);
    return GetNextItem(a_pszBuffer, a_pdwType);
}

//***************************************************
BOOL TSection::FetchToCache(LPCSTR a_pszName)
{
    FETCH_VALUE_STRUCT   *pFetch    = NULL;
    FETCH_VALUE_STRUCT   *pFetchCur = NULL;
    long                  nOffset     = 0;
    long                  nFetchCount = 0;
    long                  ctr = 0;
    TBITEM_ENTRY         *pEntry = NULL;
    std::vector<TBITEM_ENTRY*> vecEntries;

    pFetch = FetchValueByName(a_pszName);
    if (pFetch==NULL)
        return FALSE;
    //----------------------------------------------------------
    do
    {
        pFetchCur = MAKE_PTR(FETCH_VALUE_STRUCT*, pFetch, nOffset);
        nOffset = pFetchCur->ofsNext;
        vecEntries.push_back(&pFetchCur->item);
    }
    while (nOffset!=0);
    //----------------------------------------------------------
    nFetchCount = vecEntries.size();
    m_valCache.Shrink(100-nFetchCount);

    for (ctr=0; ctr<nFetchCount; ctr++)
    {
        pEntry = vecEntries[ctr];
        m_valCache.SetValue(pEntry);
    }
    free(pFetch);
    
    return TRUE;
}

//***************************************************
BOOL TSection::GetCacheValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue)
{
    TBITEM_ENTRY  *pEntry = NULL;
    TBASE_VALUE   *pValue = NULL;

    pEntry = m_valCache.FindValue(a_pszName);
    if (pEntry!=NULL)
    {
        pValue = VALUE_ItemEntry2Value(pEntry);
        if (!pValue)
            return FALSE;

        *a_ppValue = pValue;
        return TRUE;
    }
    return FALSE;
}

//***************************************************
BOOL TSection::GetValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue)
{
    TBITEM_ENTRY   *pEntry = NULL;
    TBASE_VALUE    *pValue = NULL;

    //----------------------------------------------------------
    if(!m_bCanGetValue)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    if (*a_ppValue!=NULL)
    {
        TBASE_FreeValue(*a_ppValue);
        *a_ppValue = NULL;
    }
    //----------------------------------------------------------
    // Dane po dodaniu
    pEntry = m_valForUpdate.FindValue(a_pszName);
    if (pEntry!=NULL)
    {
        if (pEntry->iType==TBVITYPE_DELETED)
        {
            TASK_SetErrorInfo(TRERR_ITEM_NOT_FOUND, a_pszName, m_strName.c_str());
            return FALSE;
        }
        else
            pValue = VALUE_ItemEntry2Value(pEntry);
            if (!pValue)
                return FALSE;
            *a_ppValue = pValue;
            return TRUE;
    }
    //----------------------------------------------------------
    // Cache
    if (GetCacheValue(a_pszName, a_ppValue))
        return TRUE;
    //----------------------------------------------------------
    // Z bazy...
    if (!FetchToCache(a_pszName))
        return FALSE;
    //----------------------------------------------------------
    return GetCacheValue(a_pszName, a_ppValue);
}

//***************************************************
BOOL TSection::FlushUpdateValues()
{
    if (m_valForUpdate.GetCount()>50)
    {
        LPVOID pvBatch = m_valForUpdate.ExportBatch();
        if (!ProcessBatch(pvBatch))
        {
            free(pvBatch);
            return FALSE;
        }
        m_valForUpdate.Clear();
    }
    return TRUE;
}

//***************************************************
BOOL TSection::SetValue(LPCSTR a_pszName, TBASE_VALUE *a_pValue)
{
    TTaskInfo*     pTaskInfo = TASK_GetCurrent();
    TPtrIter       iter;     
    LPVOID         pvBatch   = NULL; 
    FILETIME       fTime;
    TBVALUE_ENTRY *pValEntry = NULL;
    
    if (pTaskInfo==NULL)
        return NULL;

    //----------------------------------------------------------
    iter = pTaskInfo->m_setPointers.find(a_pValue);
    if (iter==pTaskInfo->m_setPointers.end() || IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    if (!Edit())
        return FALSE;
    //----------------------------------------------------------
    if (!FlushUpdateValues())
        return FALSE;
    //----------------------------------------------------------
    pValEntry = (TBVALUE_ENTRY*)&a_pValue->data;

    //----------------------------------------------------------
    if (a_pValue->dwType==TBVTYPE_DATE)
    {
        SystemTimeToFileTime(&a_pValue->data.date, &fTime);
        pValEntry = (TBVALUE_ENTRY*)&fTime;
    }
    //----------------------------------------------------------
    m_valForUpdate.SetValue(a_pszName, a_pValue->dwType, pValEntry, a_pValue->cbSize);

    return TRUE;
}

//***************************************************
BOOL TSection::LockSection(BOOL a_bLock)
{
    TCallParams   inParams(SVF_LOCK_SECTION);
    TCallParams   outParams;
    TTaskInfo    *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle (SVP_HSECTION, (HTBHANDLE)m_hSection);
    inParams.SetLong   (SVP_LOCK,     (long)a_bLock);

    return pTask->CallServer(inParams, outParams);
}

//***************************************************
BOOL TSection::Edit()
{
    if (m_bEditMode)
        return TRUE;

    if (!LockSection(TRUE))
        return FALSE;

    m_bEditMode = TRUE;
    return TRUE;
}

//***************************************************
BOOL TSection::ProcessBatch(LPVOID a_pvBatch)
{
    TCallParams   inParams(SVF_PROCESS_BATCH);
    TCallParams   outParams;
    TTaskInfo    *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle        (SVP_HSECTION, (HTBHANDLE)m_hSection);
    inParams.SetBufferPointer (SVP_BATCH,    a_pvBatch, _msize(a_pvBatch));

    return pTask->CallServer(inParams, outParams);
}

//***************************************************
BOOL TNoValueSection::GetFirstItem(TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType)
{
    FreeByIDCache();
    return TSection::GetFirstItem(a_pFilter, a_pszBuffer, a_pdwType);
}

//***************************************************
BOOL TNoValueSection::Edit()
{
    TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, NULL, m_strName.c_str());
    return FALSE;
}

//***************************************************
BOOL TNoValueSection::GetValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue)
{
    TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName, m_strName.c_str());
    return FALSE;
};

//***************************************************
void TSection::FreeByIDCache()
{
    if (m_pByID!=NULL)
    {
        free(m_pByID);
        m_pByID = NULL;
    }
}

//***************************************************
BOOL TSection::Update()
{
    LPVOID     pvBatch = NULL;
    BOOL       bRetVal = FALSE;
    TTaskInfo *pTI     = TASK_GetCurrent();
    DWORD      dwError = 0;
    TString    strErrorItem, strErrorSection;

    if (m_bEditMode==FALSE)
        return TRUE;

    pvBatch = m_valForUpdate.ExportBatch();

    bRetVal = ProcessBatch(pvBatch);

    m_valForUpdate.Clear();
    m_valCache.Clear();
    FreeByIDCache();

    if (!bRetVal)
    {
        dwError      = pTI->m_dwError;
        strErrorItem = pTI->m_strErrorItem;
        strErrorSection = pTI->m_strErrorSection;
    }
    LockSection(FALSE);

    if (!bRetVal)
    {
        pTI->m_dwError      = dwError;
        pTI->m_strErrorItem = strErrorItem;
        pTI->m_strErrorSection = strErrorSection;
    }

    m_bEditMode = FALSE;
    return bRetVal;
}

//***************************************************
BOOL TSection::CancelUpdate()
{
    LPVOID pvBatch = NULL;
    BOOL   bRetVal = FALSE;

    if (m_bEditMode==FALSE)
        return TRUE;

    m_valForUpdate.Clear();
    LockSection(FALSE);
    m_bEditMode = FALSE;

    return TRUE;
}

//***************************************************
BOOL TSection::DeleteLongBinary(LPCSTR a_pszName)
{
    TCallParams  inParams(SVF_DELETE_LONG_BINARY);
    TCallParams  outParams;
    TTaskInfo   *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle      (SVP_HSECTION, (HTBHANDLE)m_hSection);
    inParams.SetTextPointer (SVP_NAME,     a_pszName);

    return pTask->CallServer(inParams, outParams);
}

//***************************************************
BOOL TSection::DeleteValue(LPCSTR a_pszName, DWORD a_dwHint)
{
    TBITEM_ENTRY *pEntry = NULL;

    //---------------------------------------------------------
    pEntry = m_valForUpdate.FindValue(a_pszName);
    if (pEntry)
    {
        m_valForUpdate.RemoveValue(a_pszName);
        if (m_valForUpdate.GetCount()==0)
        {
            CancelUpdate();
        }
        return TRUE;
    }
    //---------------------------------------------------------
    if (a_dwHint == DELHINT_AUTO)
    {
        pEntry = m_valCache.FindValue(a_pszName);
        if (pEntry == NULL)
        {
            if (!FetchToCache(a_pszName))
                return FALSE;
            pEntry = m_valCache.FindValue(a_pszName);
        }
        if (!pEntry)
            return FALSE;

        if (TBITEM_TYPE(pEntry) == TBVTYPE_LONGBINARY)
        {
            a_dwHint = DELHINT_LONGBINARY;
        }else
            a_dwHint = DELHINT_SIMPLE;
    }
    //---------------------------------------------------------
    if (a_dwHint == DELHINT_LONGBINARY)
    {
        if (!Update())
            return FALSE;

        return DeleteLongBinary(a_pszName);
    }
    //---------------------------------------------------------
    else
    {
        if (!Edit())
            return FALSE;
        if (!FlushUpdateValues())
            return FALSE;

        m_valForUpdate.SetValueDeleted(a_pszName);
        return TRUE;
    }
}

//******************************************************************
BOOL TSection::ValueExists(LPCSTR a_pszName, BOOL *a_pbExists)
{
    BOOL bExists = FALSE;
    //----------------------------------------------------------

    if (IsBadStringPtr(a_pszName, 128) || IsBadWritePtr(a_pbExists, sizeof(BOOL)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    *a_pbExists = FALSE;

    bExists  = (m_valForUpdate.FindValue(a_pszName) != NULL);
    bExists |= (m_valCache.FindValue(a_pszName)     != NULL);

    if (!bExists)
        return TRUE;

    if (!FetchToCache(a_pszName))
        return TRUE;

    bExists = (m_valCache.FindValue(a_pszName) != NULL);
    *a_pbExists = bExists;

    return TRUE;
}

//******************************************************************
BOOL TSection::GetAccess(DWORD *a_pdwAttrs)
{
    if (IsBadWritePtr(a_pdwAttrs, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    DWORD dwAccess = TBACCESS_BROWSE;
    if(m_bCanGetValue)
        dwAccess |= TBACCESS_GETVALUE;

    if(!m_bReadOnly)
        dwAccess |= TBACCESS_WRITE;

    *a_pdwAttrs = dwAccess;
    return TRUE;
}


//******************************************************************
BOOL TSection::GetAttributes(DWORD *a_pdwAttrs)
{
    if (IsBadWritePtr(a_pdwAttrs, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    *a_pdwAttrs = m_dwAttrs;
    return TRUE;
}

//******************************************************************
BOOL TSection::GetName(BOOL a_bFull, LPSTR a_pszBuffer)
{
    long    nLen=0;
    long    nPos=0;
    TString strName;

    if (IsBadStringPtr(a_pszBuffer, 256))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, m_strName.c_str());
        return FALSE;
    }
    //----------------------------------------------------------
    if (a_bFull)
    {
        strcpy(a_pszBuffer, m_strName.c_str());
        return TRUE;
    }
    else
    {
        nLen = m_strName.length();
        nPos = m_strName.rfind('\\', nLen-2);
        strName = m_strName.substr(nPos+1, nLen-nPos-2);
        strcpy(a_pszBuffer, strName.c_str());
        return TRUE;
    }
    return FALSE;
}
