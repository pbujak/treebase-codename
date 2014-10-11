#include "stdafx.h"
#include "TBaseFile.h"
#include "calls.h"
#include "lpccodes.h"
#include "section.h"
#include "blob.h"
#include "label.h"
#include "TreeBaseSvr.h"
#include "TSystemStatus.h"
#include "Shared\TAbstractThreadLocal.h"
#include "stream.h"
#include "SecurityManager.h"
#include "junction.h"
#include "session.h"

extern SEGID_F SEGID_CURRENT_USER;
extern SEGID_F SEGID_LOCAL_MACHINE;

typedef Actor::TMessage0<TServerMainTask> TServerMainTaskMsg;

extern LPSECURITY_ATTRIBUTES G_sa;

static TThreadLocal<CString> S_strClientID;

template<>
inline void DeletePointer(TPipe *a_pPipe)
{
    a_pPipe->Close();
}

//*********************************************************
static BOOL SetOutputParam(TCallParams &a_outParams, TSection *a_pSection)
{
    if (a_pSection == NULL)
        return FALSE;

    a_outParams.SetHandle(SVP_NEW_HANDLE,  (HTBHANDLE)(HTBSECTION)a_pSection);
    a_outParams.SetText  (SVP_PATH,        a_pSection->m_strName);
    a_outParams.SetLong  (SVP_ATTRIBUTES,  a_pSection->m_dwAttrs);
    a_outParams.SetLong  (SVP_READONLY,    a_pSection->IsReadOnly());
    a_outParams.SetLong  (SVP_CANGETVALUE, a_pSection->CanGetValue());

    return TRUE;
}

//*********************************************************
static BOOL OnSetClientInfo(TCallParams &a_inParams, TCallParams &a_outParams)
{
    LPCSTR pszFileName = a_inParams.GetText(SVP_CLIENT_EXE);
    LPCSTR pszUser     = a_inParams.GetText(SVP_CLIENT_USER);
    LPCSTR pszComputer = a_inParams.GetText(SVP_CLIENT_COMPUTER);
    DWORD  dwThread    = (DWORD)a_inParams.GetLong(SVP_CLIENT_THREAD);

    TClientTask *pTask = dynamic_cast<TClientTask *>(TASK_GetCurrent());
    pTask->SetClientInfo(pszFileName, dwThread, pszComputer, pszUser);
    return TRUE;
}


//*********************************************************
static BOOL OnFlush(TCallParams &a_inParams, TCallParams &a_outParams)
{
    FILE_Flush();
    return TRUE;
}

//*********************************************************
static BOOL OnCreateSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hParent  = (HTBSECTION)a_inParams.GetHandle(SVP_HPARENT);
    LPCSTR    pszName  =            a_inParams.GetText  (SVP_NAME);
    DWORD     dwAttrs  =     (DWORD)a_inParams.GetLong  (SVP_ATTRIBUTES);
    TSection *pSection = NULL;

    pSection = SECTION_FromHandle(hParent);
    if (pSection==NULL)
        return FALSE;
    
    std::auto_ptr<Security::TSecurityAttributes> secAttrs;

    if(!Security::Facade::deserializeSecurity(a_inParams, secAttrs))
        return FALSE;

    pSection = SECTION_CreateSection(pSection, pszName, dwAttrs, secAttrs.get());

    return SetOutputParam(a_outParams, pSection);
}

//*********************************************************
static BOOL OnOpenSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hBase      = (HTBSECTION)a_inParams.GetHandle(SVP_HBASE);
    LPCSTR    pszPath    =            a_inParams.GetText  (SVP_PATH);
    DWORD     dwOpenMode =     (DWORD)a_inParams.GetLong  (SVP_OPEN_MODE);
    TSection *pSection = NULL;

    pSection = SECTION_FromHandle(hBase);
    if (pSection==NULL)
        return FALSE;
    
    pSection = SECTION_OpenSection(pSection, pszPath, dwOpenMode);

    return SetOutputParam(a_outParams, pSection);
}

//*********************************************************
static BOOL OnCloseSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    TSection *pSection = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    SECTION_CloseSection(pSection);
    return TRUE;
}

//*********************************************************
template<typename TBSTREAM, class _Stream>
static BOOL OnCloseStream(TCallParams &a_inParams, TCallParams &a_outParams)
{
    TBSTREAM hStream = (TBSTREAM)a_inParams.GetHandle(SVP_HSTREAM);

    Util::TTaskObjectSet<_Stream> sset;
    _Stream *pStream = sset.fromHandle(hStream);
    if (pStream == NULL)
        return FALSE;

    return Stream::close(pStream) ? TRUE : FALSE;
}

//*********************************************************
static BOOL OnDeleteSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HPARENT);
    LPCSTR    pszName  =            a_inParams.GetText  (SVP_NAME);
    TSection *pSection = NULL;
    
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return pSection->DeleteSection(pszName);
}

//*********************************************************
static BOOL OnRenameSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection   = (HTBSECTION)a_inParams.GetHandle(SVP_HPARENT);
    LPCSTR    pszOldName = (LPCSTR)   a_inParams.GetText  (SVP_OLDNAME);
    LPCSTR    pszNewName = (LPCSTR)   a_inParams.GetText  (SVP_NEWNAME);
    TSection *pSection = NULL;
    
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return pSection->RenameSection(pszOldName, pszNewName);
}

//*********************************************************
static BOOL OnCreateSectionLink(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSourceBase      = (HTBSECTION)a_inParams.GetHandle(SVP_HSOURCE_BASE);
    HTBSECTION  hTargetSection   = (HTBSECTION)a_inParams.GetHandle(SVP_HTARGET_SECTION);
    LPCSTR     pszSourcePath    = (LPCSTR)   a_inParams.GetText  (SVP_SOURCE_PATH);
    LPCSTR     pszLinkName      = (LPCSTR)   a_inParams.GetText  (SVP_LINKNAME);
    TSection  *pSrcBase = NULL;
    TSection  *pTarget  = NULL;

    pSrcBase = SECTION_FromHandle(hSourceBase);
    pTarget = SECTION_FromHandle(hTargetSection);
    if (pTarget==NULL)
        return FALSE;

    return pTarget->CreateLink(pSrcBase, pszSourcePath, pszLinkName);
}

//*********************************************************
static BOOL OnGetSectionSecurity(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hParent  = (HTBSECTION)a_inParams.GetHandle(SVP_HPARENT);
    LPCSTR     pszName  =             a_inParams.GetText  (SVP_NAME);
    int        nFlags   =             a_inParams.GetLong  (SVP_SECURITY_INFORMATION_FLAGS);
    TSection  *pSection = NULL;

    pSection = SECTION_FromHandle(hParent);
    if (pSection == NULL)
        return FALSE;
    
    std::auto_ptr<Security::TSecurityAttributes> attributes;
    std::auto_ptr<Util::Uuid>                    owner;
    Util::Uuid                                   protectionDomain = Util::EmptyUuid;

    if(!pSection->GetSecurity(pszName, nFlags, owner, protectionDomain, attributes))
        return FALSE;

    if(owner.get())
    {
        Security::TPrivilegedScope privilegedScope;

        ATL::CSid sidOwner = Security::Facade::getSystemUID(*owner);
        if(!sidOwner.IsValid())
            return FALSE;

        a_outParams.SetBuffer(SVP_OWNER, (PSID)sidOwner.GetPSID(), sidOwner.GetLength());
    }

    if(protectionDomain != Util::EmptyUuid)
    {
        STATIC_ASSERT(sizeof(Util::Uuid) == sizeof(GUID));
        a_outParams.SetBuffer(SVP_PROTECTION_DOMAIN, &protectionDomain, sizeof(Util::Uuid));
    }

    if(!Security::Facade::serializeSecurity(a_outParams, attributes))
        return FALSE;

    return TRUE;
}

//*********************************************************
static BOOL OnSetSectionSecurity(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hParent  = (HTBSECTION)a_inParams.GetHandle(SVP_HPARENT);
    LPCSTR     pszName  =             a_inParams.GetText  (SVP_NAME);

    TSection  *pSection = SECTION_FromHandle(hParent);
    if (pSection == NULL)
        return FALSE;

    TBSECURITY_TARGET::Enum    target    = (TBSECURITY_TARGET::Enum)   a_inParams.GetLong(SVP_SECURITY_TARGET);
    TBSECURITY_OPERATION::Enum operation = (TBSECURITY_OPERATION::Enum)a_inParams.GetLong(SVP_SECURITY_OPERATION);

    std::auto_ptr<Security::TSecurityAttributes> secAttrs;

    if(!Security::Facade::deserializeSecurity(a_inParams, secAttrs))
        return FALSE;

    return pSection->SetSecurity(pszName, target, operation, *secAttrs);
}

//*********************************************************
static BOOL OnGetSectionLinkCount(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection   = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    long       nLinkCount = -1; 
    TSection  *pSection   = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    nLinkCount = pSection->GetLinkCount();
    if (nLinkCount!=-1)
    {
        a_outParams.SetLong(SVP_LINK_COUNT, nLinkCount);
        return TRUE;
    }
    return FALSE;
}


//*********************************************************
static BOOL OnGetFirstItems(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION         hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    TSection          *pSection = NULL;
    LPVOID             pvItems = NULL;
    TBASE_ITEM_FILTER *pFilter  = NULL;
    long               cbFilterSize = 0; 

    pSection = SECTION_FromHandle(hSection);
    if (pSection == NULL)
        return FALSE;

    a_inParams.GetBuffer(SVP_FILTER, (LPVOID*)&pFilter, cbFilterSize);

    pvItems = pSection->GetFirstItems(pFilter);

    a_outParams.EatBuffer(SVP_ITEMS, pvItems);
    return TRUE;
}

//*********************************************************
static BOOL OnGetNextItems(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    TSection   *pSection = NULL;
    LPVOID      pvItems = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection == NULL)
        return FALSE;

    pvItems = pSection->GetNextItems();

    a_outParams.EatBuffer(SVP_ITEMS, pvItems);
    return TRUE;
}

//*********************************************************
static BOOL OnFetchValueById(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION           hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    DWORD               dwHash   = (DWORD)    a_inParams.GetLong  (SVP_HASH); 
    WORD                wOrder   = (WORD)     a_inParams.GetLong  (SVP_ORDER); 
    FETCH_VALUE_STRUCT *pFetch   = NULL;
    BOOL                bRetVal  = FALSE;

    TSection  *pSection = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    pFetch = (FETCH_VALUE_STRUCT *)pSection->FetchValueByID(dwHash, wOrder);
    if (!pFetch)
        return FALSE;

    a_outParams.EatBuffer(SVP_FETCH_VALUE, pFetch);
    return TRUE;
}

//*********************************************************
static BOOL OnFetchValueByName(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION           hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR              pszName  =            a_inParams.GetText  (SVP_NAME); 
    FETCH_VALUE_STRUCT *pFetch   = NULL;
    BOOL                bRetVal  = FALSE;

    TSection  *pSection = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    pFetch = (FETCH_VALUE_STRUCT *)pSection->FetchValueByName(pszName);
    if (!pFetch)
        return FALSE;

    a_outParams.EatBuffer(SVP_FETCH_VALUE, pFetch);
    return TRUE;
}

//*********************************************************
static BOOL OnLockSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    BOOL      bLock    = (BOOL)     a_inParams.GetLong  (SVP_LOCK);  

    TSection *pSection = NULL;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    if (bLock)
    {
        if (!pSection->Lock())
            return FALSE;
    }
    else
        pSection->Unlock();
    return TRUE;
}

//*********************************************************
static BOOL OnProcessBatch(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPVOID    pvBatch  = NULL;  
    long      cbSize   = 0;  

    TSection *pSection = NULL;

    if (!a_inParams.GetBuffer(SVP_BATCH, &pvBatch, cbSize))
        return FALSE;

    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return pSection->ProcessBatch(pvBatch);
}

//*********************************************************
static BOOL OnDeleteLongBinary(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION    hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR       pszName  =            a_inParams.GetText  (SVP_NAME);  

    TSection *pSection = NULL;
    //--------------------------------------------------------
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return pSection->DeleteLongBinary(pszName);
}

//*********************************************************
static BOOL OnRenameLongBinary(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION hSection   = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR    pszOldName = (LPCSTR)   a_inParams.GetText  (SVP_OLDNAME);
    LPCSTR    pszNewName = (LPCSTR)   a_inParams.GetText  (SVP_NEWNAME);
    TSection *pSection = NULL;
    
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return pSection->RenameLongBinary(pszOldName, pszNewName);
}

//*********************************************************
static BOOL OnLinkLongBinary(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSourceSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSOURCE_SECTION);
    HTBSECTION  hTargetSection = (HTBSECTION)a_inParams.GetHandle(SVP_HTARGET_SECTION);
    LPCSTR     pszName        = (LPCSTR)   a_inParams.GetText  (SVP_NAME);
    LPCSTR     pszLinkName    = (LPCSTR)   a_inParams.GetText  (SVP_LINKNAME);
    TSection  *pSource = NULL;
    TSection  *pTarget = NULL;

    pSource = SECTION_FromHandle(hSourceSection);
    if (pSource==NULL)
        return FALSE;
    pTarget = SECTION_FromHandle(hTargetSection);
    if (pTarget==NULL)
        return FALSE;

    return pTarget->CreateLinkLB(pSource, pszName, pszLinkName);
}


//*********************************************************
static BOOL OnSectionExists(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION    hBase   = (HTBSECTION)a_inParams.GetHandle(SVP_HBASE);
    LPCSTR       pszPath =            a_inParams.GetText  (SVP_PATH);  
    BOOL         bExist  = FALSE;

    TSection *pSection = NULL;
    //--------------------------------------------------------
    pSection = SECTION_FromHandle(hBase);

    if (!TSection::SectionExists(pSection, pszPath, bExist))
        return FALSE;

    a_outParams.SetLong(SVP_EXIST, (long)bExist);

    return TRUE;
}

//*********************************************************
static BOOL OnGetSectionLabel(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    CString    strLabel;

    TSection  *pSection = NULL;
    //--------------------------------------------------------
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    if (!LABEL_GetSectionLabel(pSection, strLabel))
        return FALSE;

    a_outParams.SetText(SVP_LABEL, strLabel);

    return TRUE;
}

//*********************************************************
static BOOL OnSetSectionLabel(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR     pszLabel =            a_inParams.GetText  (SVP_LABEL);

    TSection  *pSection = NULL;
    //--------------------------------------------------------
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return LABEL_SetSectionLabel(pSection, pszLabel);
}

//*********************************************************
static BOOL OnDeleteSectionLabel(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);

    TSection  *pSection = NULL;
    //--------------------------------------------------------
    pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return LABEL_DeleteSectionLabel(pSection);
}

//*********************************************************
static BOOL OnCreateLongBinary(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR     pszName  = (LPCSTR)   a_inParams.GetText  (SVP_NAME);

    TSection* pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    Stream::TOutputStream *pLB = SECTION_CreateLongBinary(pSection, pszName);
    a_outParams.SetHandle(SVP_HSTREAM, (HTBHANDLE)pLB);

    return pLB != NULL;
}

//*********************************************************
static BOOL OnOpenLongBinary(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR     pszName  = (LPCSTR)   a_inParams.GetText  (SVP_NAME);

    TSection* pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    Stream::TInputStream *pLB = SECTION_OpenLongBinary(pSection, pszName);
    a_outParams.SetHandle(SVP_HSTREAM, (HTBHANDLE)pLB);

    return pLB != NULL;
}

//*********************************************************
static BOOL OnGetStreamData(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSTREAMIN  hStream  = (HTBSTREAMIN)a_inParams.GetHandle(SVP_HSTREAM);
    DWORD        cbCount  =              a_inParams.GetLong  (SVP_COUNT);  

    Util::TTaskObjectSet<Stream::TInputStream> isset;
    Stream::TInputStream *pStream = isset.fromHandle(hStream);
    if(pStream == NULL)
        return FALSE;

    void *pvBuff = pStream->read(cbCount);
    if(pvBuff == NULL)
        return FALSE;

    a_outParams.EatBuffer(SVP_BUFFER, pvBuff);
    return TRUE;
}

//*********************************************************
static BOOL OnPutStreamData(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSTREAMOUT hStream = (HTBSTREAMOUT)a_inParams.GetHandle(SVP_HSTREAM);
    LPVOID       pvBuff  = NULL;  
    long         cbSize   = 0;  

    if (!a_inParams.GetBuffer(SVP_BUFFER, &pvBuff, cbSize))
        return FALSE;

    Util::TTaskObjectSet<Stream::TOutputStream> isset;
    Stream::TOutputStream *pStream = isset.fromHandle(hStream);
    if(pStream == NULL)
        return FALSE;

    cbSize = pStream->write(pvBuff, cbSize);

    a_outParams.SetLong(SVP_COUNT, cbSize);

    return TRUE;
}

//*********************************************************
static BOOL OnCreateBlobTempSection(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR     pszName  = (LPCSTR)   a_inParams.GetText  (SVP_NAME);

    TSection* pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    pSection = BLOB_CreateBlobTempSection(pSection, pszName);

    return SetOutputParam(a_outParams, pSection);
}

//*********************************************************
static BOOL OnCommitBlob(TCallParams &a_inParams, TCallParams &a_outParams)
{
    HTBSECTION  hSection       = (HTBSECTION)a_inParams.GetHandle(SVP_HSECTION);
    LPCSTR     pszName        = (LPCSTR)   a_inParams.GetText  (SVP_NAME);
    LPCSTR     pszTempSection = (LPCSTR)   a_inParams.GetText  (SVP_TEMP_SECTION);

    TSection* pSection = SECTION_FromHandle(hSection);
    if (pSection==NULL)
        return FALSE;

    return BLOB_CommitBlob(pSection, pszName, pszTempSection);
}

//*********************************************************
static BOOL InitModule()
{
    InitializeCriticalSection(&TClientTask::S_cs);
    return TRUE;
}

//*************************************************************************
CString TASK_GetComputerName()
{
    TClientTask *pTask = dynamic_cast<TClientTask *>(TASK_GetCurrent());
    if(pTask)
        return pTask->m_strComputer;

    
    char szComputer[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD dwSize = sizeof(szComputer);
    ::GetComputerName(szComputer, &dwSize);
    return CString(szComputer);
};


IMPLEMENT_INIT(InitModule)

CMap<DWORD, DWORD, DWORD, DWORD> TClientTask::S_mapClientThreads; 
CRITICAL_SECTION                 TClientTask::S_cs = {0};


//*********************************************************
BEGIN_CALLSERVER_DISPATCH()
    CSD_(SVP_SET_CLIENT_INFO,        OnSetClientInfo)
    CSD_(SVF_FLUSH,                  OnFlush)
    CSD_(SVF_CREATE_SECTION,         OnCreateSection)
    CSD_(SVF_OPEN_SECTION,           OnOpenSection)
    CSD_(SVF_CLOSE_SECTION,          OnCloseSection)
    CSD_(SVF_DELETE_SECTION,         OnDeleteSection)
    CSD_(SVF_RENAME_SECTION,         OnRenameSection)
    CSD_(SVF_CREATE_SECTION_LINK,    OnCreateSectionLink)
    CSD_(SVF_GET_FIRST_ITEMS,        OnGetFirstItems)
    CSD_(SVF_GET_NEXT_ITEMS,         OnGetNextItems)
    CSD_(SVF_FETCH_VALUE_BY_ID,      OnFetchValueById)
    CSD_(SVF_FETCH_VALUE_BY_NAME,    OnFetchValueByName)
    CSD_(SVF_LOCK_SECTION,           OnLockSection)
    CSD_(SVF_PROCESS_BATCH,          OnProcessBatch)
    CSD_(SVF_DELETE_LONG_BINARY,     OnDeleteLongBinary)
    CSD_(SVF_RENAME_LONG_BINARY,     OnRenameLongBinary)
    CSD_(SVF_LINK_LONG_BINARY,       OnLinkLongBinary)
    CSD_(SVF_SECTION_EXISTS,         OnSectionExists)
    CSD_(SVF_GET_SECTION_LABEL,      OnGetSectionLabel) 
    CSD_(SVF_SET_SECTION_LABEL,      OnSetSectionLabel)   
    CSD_(SVF_DELETE_SECTION_LABEL,   OnDeleteSectionLabel)  
    CSD_(SVF_GET_SECTION_LINK_COUNT, OnGetSectionLinkCount)
    CSD_(SVF_CREATE_LONG_BINARY,     OnCreateLongBinary)
    CSD_(SVF_OPEN_LONG_BINARY,       OnOpenLongBinary)
    CSD_(SVF_GET_DATA,               OnGetStreamData)
    CSD_(SVF_PUT_DATA,               OnPutStreamData)
    CSD2_(SVF_CLOSE_STREAMIN,        OnCloseStream, HTBSTREAMIN, Stream::TInputStream)
    CSD2_(SVF_CLOSE_STREAMOUT,       OnCloseStream, HTBSTREAMOUT, Stream::TOutputStream)
    CSD_(SVF_CREATE_BLOB_TEMP_SECTION, OnCreateBlobTempSection)
    CSD_(SVF_COMMIT_BLOB,            OnCommitBlob)
    CSD_(SVF_GET_SECTION_SECURITY,   OnGetSectionSecurity)
    CSD_(SVF_SET_SECTION_SECURITY,   OnSetSectionSecurity)
END_CALLSERVER_DISPATCH()

//*********************************************************
TClientTask::~TClientTask()
{
    if(m_pvBlobBuf)
        UnmapViewOfFile(m_pvBlobBuf);
    if (m_hBlobBuf!=NULL)
        CloseHandle(m_hBlobBuf);

    _release_ptr(m_pPipe);
};

//*********************************************************
BOOL TClientTask::OnStart()
{
    m_hBlobBuf = m_pPipe->CreateSharedMemoryForBlob(true);
    if (m_hBlobBuf==NULL)
        return FALSE;

    m_pvBlobBuf = MapViewOfFile(m_hBlobBuf, FILE_MAP_WRITE, 0, 0, 0);
    if(m_pvBlobBuf==NULL)
        return FALSE;

    EnterCriticalSection(&S_cs);
    S_mapClientThreads.SetAt(m_dwClientThreadId, m_dwClientThreadId);
    LeaveCriticalSection(&S_cs);
    
    return TRUE;
};

//*********************************************************
void TClientTask::OnBeforeRun()
{
    TTask::OnBeforeRun();
    G_dbManager.LockShared();

    TServerMainTaskMsg *pMsgRelease = new TServerMainTaskMsg();
    TServerMainTask *pMain = TServerMainTask::getInstance();
    pMsgRelease->Send(pMain, &TServerMainTask::AddRef);
}

//*********************************************************
void TClientTask::OnRun()
{
    TCallParams inParams, outParams;

    if(!m_pPipe->SyncBarrier())
        return;

    try
    {
        while(true)
        {
            inParams.Read(m_pPipe);
            CallServer(inParams, outParams);
            outParams.Write(m_pPipe);
            m_pPipe->Commit();
            inParams.Reset();
            outParams.Reset();
        }
    }catch (TPipeException *)
    {
    }
    _release_ptr(m_pPipe);
}

//*********************************************************
void TClientTask::CallServer(TCallParams &a_inParams, TCallParams &a_outParams)
{
    BOOL  bRetVal = FALSE;

    m_dwError = -1;
    m_strErrorItem.Empty();
    m_strErrorSection.Empty();

    try
    {
        if(m_bAuthenticated || a_inParams.m_nFunction == SVP_SET_CLIENT_INFO)
        {
            bRetVal = _CallServerDispatch(a_inParams, a_outParams);
        }
        else
        {
            m_dwError = TRERR_ACCESS_DENIED;
        }
    }
    catch(Storage::TStorageException *e)
    {
        e->Delete();
        m_dwError = TRERR_STORAGE_FAILURE;
        bRetVal = FALSE;
    }

    a_outParams.SetLong       (SVP_SYS_ERRORCODE,    (long)m_dwError);
    a_outParams.SetTextPointer(SVP_SYS_ERRORITEM,    m_strErrorItem);
    a_outParams.SetTextPointer(SVP_SYS_ERRORSECTION, m_strErrorSection);
    a_outParams.SetLong       (SVP_SYS_RETURNVALUE,  (long)bRetVal);
}

//*************************************************************
void TClientTask::OnFinish()
{
    if(m_bAuthenticated)
    {
        Session::releaseUserInfo(m_sid);
        Session::releaseComputerInfo(m_strComputer);
    }

    System::removeSession(S_strClientID.Value());

    TTask::OnFinish();
    G_dbManager.UnlockShared();

    TServerMainTaskMsg *pMsgRelease = new TServerMainTaskMsg();
    TServerMainTask *pMain = TServerMainTask::getInstance();
    pMsgRelease->Send(pMain, &TServerMainTask::Release);
}

//*************************************************************
void TClientTask::SetClientInfo(LPCSTR a_pszClientExe, DWORD a_dwClientThread, LPCSTR a_pszComputer, LPCSTR a_pszUser)
{
    m_bAuthenticated = TRUE;

    m_sid.LoadAccount(m_pPipe->GetClientUserId(), a_pszComputer);

    S_strClientID = System::addSession(Util::message("%s;%s;%s;%X", a_pszComputer, a_pszUser, a_pszClientExe, a_dwClientThread));

    Session::UserInfo userInfo;
    Session::getUserInfo(m_sid, userInfo);

    if(userInfo.junctionSegId != SEGID_NULL)
        Junction::addLocalJunctionPoint(SEGID_CURRENT_USER, userInfo.junctionSegId);

    m_strComputer = a_pszComputer;
    SEGID_F segIdComputer = {0,0};
    Session::getComputerInfo(m_strComputer, segIdComputer);

    if(segIdComputer != SEGID_NULL)
        Junction::addLocalJunctionPoint(SEGID_LOCAL_MACHINE, segIdComputer);

    Security::TTaskSecurityContext *pContext = Security::Manager::GetTaskContext();
    pContext->user = userInfo.uid;

    if(m_bAdmin)
    {
        pContext->privilege = Security::TTaskSecurityContext::PrivilegeLevel::eAdmin;
    }
    else if(userInfo.uid == Security::Facade::EmergencyUID)
    {
        pContext->privilege = Security::TTaskSecurityContext::PrivilegeLevel::eEmergency;
    }
    else
    {
        pContext->privilege = Security::TTaskSecurityContext::PrivilegeLevel::eUser;
    }
}
