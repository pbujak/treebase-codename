// TreeBaseSvr.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "TTask.h"
#include "TBaseFile.h"
#include "SegmentMgr.h"
#include "TreeBaseInt.h"
#include <Shlwapi.h>
#include <atlsecurity.h>
#include "lpc.h"
#include "calls.h"
#include "Storage.h"
#include "TreeBaseSvr.h"
#include "TSystemStatus.h"
#include "util.h"
#include "junction.h"
#include "TCompactTask.h"
#include "blob.h"
#include "SecurityManager.h"
#include "TSecurityAttributes.h"

static const SEGID_F MOUNT_SEGID_ROOT    = {-1, 0};
static const SEGID_F MOUNT_SEGID_STORAGE = {-1, -1};

SEGID_F SEGID_CURRENT_USER  = {-1, 10};
SEGID_F SEGID_LOCAL_MACHINE = {-1, 11};

BOOL CONFIG_Init();

LPSECURITY_ATTRIBUTES     G_sa = NULL;
TServerEntry              G_entry;          
TTaskMemAlloc             G_taskMalloc;
TAppMemAlloc              G_appMalloc;
ATL::CSecurityAttributes  G_saAdmin;

static HANDLE        G_hGlobalMutex = NULL;

HANDLE G_hShutDownEvent = NULL;

#pragma comment(lib, "Shlwapi.lib")

static void DoServerEntry();

//************************************************************************
// class  TServerErrorInfo
class  TServerErrorInfo : public TErrorInfo
{
public:
    virtual void SetTaskErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection);
};

//************************************************************************
void TServerErrorInfo::SetTaskErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)
{
    TASK_SetErrorInfo(a_dwError, a_pszErrorItem, a_pszSection);
};

static TServerErrorInfo G_ErrorInfo;


//************************************************************************
static void SecInit()
{
    static SECURITY_DESCRIPTOR sd = { 0 };

    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, 0, FALSE);

    static SECURITY_ATTRIBUTES sa = { 0 };
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = &sd;

    G_sa = &sa;

    ATL::CDacl dacl;

    dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL);
    dacl.AddAllowedAce(Sids::CreatorOwner(), GENERIC_ALL);

    ATL::CSecurityDesc adminSD;
    adminSD.SetDacl(dacl);
    ATL::CSecurityAttributes adminSA;
    G_saAdmin.Set(adminSD);
}

//************************************************************************
class TTestTask: public TTask
{
    virtual void OnRun();
};

//*************************************************************
void TTestTask::OnRun()
{
/*
    CString strLabel;
    TSection *pSection = SECTION_OpenSection(GetRootSection(), "\\Pracownicy");
    if(pSection == NULL)
    {
        pSection = SECTION_CreateSection(GetRootSection(), "Pracownicy", 0, NULL);
        TValueMap xMap;

        long ctr;
        char szBuff[1000];
        char szName[100];
        TBVALUE_ENTRY* pVal = (TBVALUE_ENTRY*)szBuff;
        LPVOID pvBatch = NULL;


        for (ctr=0; ctr<400; ctr++)
        {
            wsprintf(szName, "Wiek %d", ctr);
            pVal->int32 = ctr;
            xMap.SetValue(szName, TBVTYPE_INTEGER, pVal);

            wsprintf(szName, "Nazwisko %d", ctr);
            strcpy(pVal->text, "Pawe³ Bujak");
            xMap.SetValue(szName, TBVTYPE_TEXT, pVal);

            wsprintf(szName, "Adres %d", ctr);
            strcpy(pVal->text, "Wallisa 2/5");
            xMap.SetValue(szName, TBVTYPE_TEXT, pVal);
        }

        pvBatch = xMap.ExportBatch();
        pSection->ProcessBatch(pvBatch);
    }
    else
    {
        void *pvData = pSection->FetchValueByName("Adres 390");
        int s = 0;
    }

    pSection->DeleteSection("P3");

    

    TLongBinary *pLB = pSection->OpenLongBinary("Image.bmp");

    char *str = (char*)malloc(150000);

//    long ctr=0;
//    for (ctr=0; ctr<150000; ctr++)
//    {
//        str[ctr] = 'A' + ctr % 20;
//    }

    DWORD cbSize = 40000;
//    pLB->Read(125, cbSize, str);

    Sleep(400000);
*/
}


//************************************************************************
void _test()
{
    TTask *pTask = NULL;

    pTask = new TTestTask();
    pTask->Start();
}

//************************************************************************
static SEGID_F GetDBRootSegment(int a_dbID)
{
    SEGID_F segId;

    if(a_dbID == -1)
    {
        segId.ndbId = -1;
        segId.nSegId = -1;
    }
    else
    {
        segId.ndbId = a_dbID;
        segId.nSegId = SEGMENT_GetRootSection(a_dbID);
    }
    return segId;
}

//************************************************************************
static BOOL OpenBaseFile(DBIDS &a_dbIds)
{
    char szPath[256]     = {0};
    char szBaseFile[256] = {0};
    SEGID_F segIdMntRoot = {-1,-1};

    GetModuleFileName(NULL, szPath, 255);
    PathRemoveFileSpec(szPath);
    PathCombine(szBaseFile, szPath, "Data\\treebase.dat");

    a_dbIds.mainDB = FILE_MountDatabase(MOUNT_SEGID_STORAGE, "", szBaseFile);

    if(a_dbIds.mainDB == -1)
        return FALSE;

    PathCombine(szBaseFile, szPath, "Data\\users.dat");
    a_dbIds.userDB = FILE_MountDatabase(MOUNT_SEGID_STORAGE, "", szBaseFile);
    if(a_dbIds.userDB == -1)
        System::setAlarm("DATABASE", "Cannot open file \"users.dat\"");

    PathCombine(szBaseFile, szPath, "Data\\databases.dat");
    a_dbIds.dbDB = FILE_MountDatabase(MOUNT_SEGID_STORAGE, "", szBaseFile);
    if(a_dbIds.dbDB == -1)
        System::setAlarm("DATABASE", "Cannot open file \"databases.dat\"");

    return TRUE;
}

TServerMainTask* TServerMainTask::s_pInstance = NULL;

//************************************************************************
BOOL TServerMainTask::OnStart()
{
    COMMON_Initialize(&G_taskMalloc, &G_ErrorInfo);

    System::TSystemStatus::GetInstance().StartHandler();

    if(!InitSecurity())
        return FALSE;

    G_hGlobalMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, COMMON_GetGlobalMutexName());
    if(G_hGlobalMutex)
    {
        m_pContext->error("TreeBase Engine already running");
        return FALSE;
    }

    if(!G_entry.Create(G_sa, &G_saAdmin))
    {
        m_pContext->error("You have not enough privileges to run TreeBaseServer.");
        return FALSE;
    }

    if (!InitNamespace())
        return FALSE;

    DBIDS dbIds;
    if (OpenBaseFile(dbIds))
        EnableBaseFile(dbIds);

    if(!CONFIG_Init())
        return FALSE;

    G_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    G_hGlobalMutex = CreateMutex(G_sa, FALSE, COMMON_GetGlobalMutexName());

    // Compact
    m_pCompactTask = new TCompactTask();
    if(!m_pCompactTask->Start())
    {
        delete m_pCompactTask;
        return FALSE;
    }

    MapExtraHandle(G_entry.m_hInitEvent, &m_mfClientConnection);
    MapExtraHandle(G_entry.m_hInitEventAdmin, &m_mfAdminClientConnection);
    m_pContext->setState(TRunningContext::eRunning);
    return TRUE;
}

//************************************************************************
void TServerMainTask::addJuction(TSection *a_pBase, LPCSTR a_pszSection, const SEGID_F &a_destSegID)
{
    if(a_destSegID.ndbId == -1 && a_destSegID.nSegId == -1)
        return;

    Util::TSectionPtr refSection = SECTION_CreateSection(a_pBase, a_pszSection, TBATTR_NOVALUES, NULL);
    if(refSection == NULL)
        return;

    SEGID_F srcSegID = {refSection->m_ndbId, refSection->m_nSegId};
    refSection->Close();

    Junction::addGlobalJunctionPoint(srcSegID, a_destSegID);
}

//************************************************************************
void TServerMainTask::EnableBaseFile(const DBIDS &a_dbIds)
{
    addJuction(GetRootSection(), "RootStorage", GetDBRootSegment(a_dbIds.mainDB));
    addJuction(GetRootSection(), "CurrentUser", SEGID_CURRENT_USER);
    addJuction(GetRootSection(), "LocalMachine", SEGID_LOCAL_MACHINE);
    addJuction(GetRootSection(), "Databases", GetDBRootSegment(a_dbIds.dbDB));

    Util::TSectionPtr refRootStorage = SECTION_OpenSection(GetRootSection(), "RootStorage", TBOPEN_MODE_DYNAMIC);
    if(refRootStorage)
    {
        Util::TSectionPtr refSection = SECTION_CreateSection(refRootStorage, "Computers", TBATTR_NOVALUES, NULL);
        
        refSection = SECTION_CreateSection(refRootStorage, "System", TBATTR_NOVALUES, NULL);
        if(refSection)
        {
            //Security::TSecurityAttributes authSecurity(&Security::Manager::AuthSecurity);
            Util::TSectionPtr authSection = SECTION_CreateSection(refSection, "Authentication", TBATTR_NOVALUES, NULL/*&authSecurity*/);
            if(!authSection)
                System::setAlarm("DATABASE", "Cannot open \\RootStorage\\System\\Authentication");
        }
        else
            System::setAlarm("DATABASE", "Cannot open \\RootStorage\\System");

        addJuction(refRootStorage, "Users", GetDBRootSegment(a_dbIds.userDB));
        addJuction(refRootStorage, "Databases", GetDBRootSegment(a_dbIds.dbDB));
        //addJuction(refRootStorage, "Temp", GetDBRootSegment(FILE_dbIdTemp));
    }
}

//************************************************************************
BOOL TServerMainTask::InitSecurity()
{
    SecInit();

    return TRUE;
}

//************************************************************************
BOOL TServerMainTask::InitNamespace()
{
    long idx = G_dbManager.AddEntry(MOUNT_SEGID_ROOT, "", "");
    if(G_dbManager.MountFile(idx) != NULL)
    {
//        Security::TSecurityAttributes systemSecurity(&Security::Manager::SystemSecurity);
        Security::TSecurityAttributes tempSecurity(&Security::Manager::TempSecurity);
//        Security::TSecurityAttributes authSecurity(&Security::Manager::AuthSecurity);

        Util::TSectionPtr pSection = SECTION_CreateSection(SECTION_FromHandle(TBSECTION_ROOT), "System", TBATTR_NOVALUES, NULL);
        if(!pSection)
            return FALSE;

        Util::TSectionPtr pSection1 = SECTION_CreateSection(pSection, "Temp", TBATTR_NOVALUES, &tempSecurity);
        if(!pSection1)
            return FALSE;

        pSection1 = SECTION_CreateSection(pSection, "UserMapping", 0, NULL);
        if(!pSection1)
            return FALSE;

        pSection = SECTION_CreateSection(pSection, "Status", TBATTR_NOVALUES, NULL);
        if(!pSection)
            return FALSE;

        pSection1 = SECTION_CreateSection(pSection, "Alarms", 0, NULL);
        if(!pSection1)
            return FALSE;

        pSection1 = SECTION_CreateSection(pSection, "Sessions", 0, NULL);
        if(!pSection1)
            return FALSE;

        pSection1 = SECTION_CreateSection(pSection, "Databases", 0, NULL);
        if(!pSection1)
            return FALSE;

        if(!FILE_InitTempDatabase())
        {
            m_pContext->error("Temporary storage initialization failed.");
            return FALSE;
        }
        //FILE_dbIdTemp = FILE_MountDatabase(MOUNT_SEGID_STORAGE, "", "$TEMP");

        return TRUE;
    };  

    m_pContext->error("Root initialization failed.");
    return FALSE;
}

//************************************************************************
void TServerMainTask::OnBeforeRun()
{
}

//************************************************************************
void TServerMainTask::HandleClientConnection(bool a_isAdmin)
{
    SERVER_ENTRY_BLOCK *pBlock = G_entry.GetBlock();
    BOOL                bExist = FALSE;

    if(pBlock)
    {
        // ... Do wype³nienia
        DWORD dwClientThreadId = pBlock->dwThreadId;

        ATL::CSid sid( *(SID*)pBlock->clientSid );

        EnterCriticalSection(&TClientTask::S_cs);
        bExist = TClientTask::S_mapClientThreads.Lookup(dwClientThreadId, dwClientThreadId);
        LeaveCriticalSection(&TClientTask::S_cs);

        if(!bExist)
        {
            std::auto_ptr<TPipeFactory> ppf(TPipeFactory::GetInstanceForSharedMemory(dwClientThreadId, sid));

            TClientTask *pTask = NULL;
            TPipe *pPipe = ppf->CreateAtServer();

            BOOL bOK = FALSE;
            if(pPipe)
            {
                pPipe->AddWaitObject(G_hShutDownEvent);
                TClientTask *pTask = new TClientTask(pPipe, a_isAdmin);
                bOK = pTask->Start();
            }
            if (!bOK)
            {
                _release_ptr(pTask);
                pBlock->bResult = 0;
            }
            else
                pBlock->bResult = 1;
        }
        else
        {
            pBlock->bResult = 1;
        }
        G_entry.ReleaseBlock();
        SetEvent(G_entry.m_hReplyEvent);
    }
}

//************************************************************************
void TServerMainTask::Stop()
{
    m_pContext->setState(TRunningContext::eStopping);
    System::TSystemStatus::GetInstance().StopHandler();
    m_pCompactTask->Stop();
    G_dbManager.LockExclusive();

    for(int i = 0; i < G_dbManager.GetCount(); i++)
    {
        G_dbManager.UmountFile(i);
    }
    m_pContext->setState(TRunningContext::eStopped);
    Actor::TMessageQueueTask::Stop();
}

//************************************************************************
void TServerMainTask::ShutDown()
{
    SetEvent(G_hShutDownEvent);
    System::TSystemStatus::GetInstance().StopHandler();
    m_pCompactTask->Stop();
    G_dbManager.LockExclusive();

    for(int i = 0; i < G_dbManager.GetCount(); i++)
    {
        G_dbManager.UmountFile(i);
    }
    m_pContext->setState(TRunningContext::eShutDownReady);
}

//************************************************************************
void TServerMainTask::AddRef()
{
    if((m_nRefCount++) == 0)
        m_pContext->enableStop(false);
}

//************************************************************************
void TServerMainTask::Release()
{
    if((--m_nRefCount) == 0)
        m_pContext->enableStop(true);
}
