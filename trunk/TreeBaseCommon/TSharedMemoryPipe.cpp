#include "stdafx.h"
#include "TSharedMemoryPipe.h"
#include <memory>
#include <assert.h>
#include "TreeBaseInt.h"

static LPCSTR NAME_FORMAT_SEM_READ    = KOBJNAME("TreeBase Server LPC SemRead(%X,%d)");
static LPCSTR NAME_FORMAT_SEM_WRITE   = KOBJNAME("TreeBase Server LPC SemWrite(%X,%d)");
static LPCSTR NAME_FORMAT_DISC_MUTEX  = KOBJNAME("TreeBase Server LPC DiscMutex(%X,%d)");
static LPCSTR NAME_FORMAT_READY_EVENT = KOBJNAME("TreeBase Server LPC ReadyEvent(%X,%d)");
static LPCSTR NAME_FORMAT_SHARED_MEM  = KOBJNAME("TreeBase Server LPC ExchangeBuffer(%X)");

//************************************************************************
TSharedMemoryPipe::TSharedMemoryPipe(DWORD a_dwThreadID): 
    m_dwThreadID(a_dwThreadID), m_bReady(false),
    m_nExtraHandleCount(0)
{
    m_hSemWrite  = NULL;
    m_hSemRead   = NULL;
    m_hSemWrite2 = NULL; 
    m_hSemRead2  = NULL;

    m_hShared = NULL;
    m_pvShared = NULL;
    m_hDiscMutex = NULL;
    m_hDiscMutex2 = NULL;

    m_hReadyEvent = NULL;
    m_hReadyEvent2 = NULL;

    memset(&m_buffIn, 0, sizeof(PIPE_BUFFER));
    memset(&m_buffOut, 0, sizeof(PIPE_BUFFER));
    memset(m_hExtraHandles, 0, sizeof(m_hExtraHandles));
    m_nSendOfs = 0; 
    m_nReceiveOfs = 0;  
};

//************************************************************************
TSharedMemoryPipe::~TSharedMemoryPipe()
{
    if (m_pvShared)
        UnmapViewOfFile(m_pvShared);

    if(m_hDiscMutex2)
        ReleaseMutex(m_hDiscMutex2);

    CloseHandles();
}

//************************************************************************
void TSharedMemoryPipe::GetObjectNames(DWORD a_dwKey, bool a_bServer, LPC_OBJECT_NAMETAB *a_pObjectNames)
{
    int nOwn  = a_bServer ? 1 : 0;
    int nPeer = nOwn ^ 1;

    wsprintf(a_pObjectNames->m_szSemOwnRead,  NAME_FORMAT_SEM_READ, a_dwKey, nOwn);
    wsprintf(a_pObjectNames->m_szSemPeerRead, NAME_FORMAT_SEM_READ, a_dwKey, nPeer);

    wsprintf(a_pObjectNames->m_szSemOwnWrite,  NAME_FORMAT_SEM_WRITE, a_dwKey, nOwn);
    wsprintf(a_pObjectNames->m_szSemPeerWrite, NAME_FORMAT_SEM_WRITE, a_dwKey, nPeer);

    wsprintf(a_pObjectNames->m_szDiscMutexOwnName,  NAME_FORMAT_DISC_MUTEX, a_dwKey, nOwn);
    wsprintf(a_pObjectNames->m_szDiscMutexPeerName, NAME_FORMAT_DISC_MUTEX, a_dwKey, nPeer);

    wsprintf(a_pObjectNames->m_szOwnReadyEventName,  NAME_FORMAT_READY_EVENT, a_dwKey, nOwn);
    wsprintf(a_pObjectNames->m_szPeerReadyEventName, NAME_FORMAT_READY_EVENT, a_dwKey, nPeer);

    wsprintf(a_pObjectNames->m_szSharedMemName,  NAME_FORMAT_SHARED_MEM, a_dwKey);
}

//************************************************************************
void TSharedMemoryPipe::InitSecurityAttributes(ATL::CSecurityAttributes &a_secAttrs)
{
    ATL::CDacl dacl;
    dacl.AddAllowedAce(m_sid, GENERIC_ALL);
    ATL::CSecurityDesc desc;
    desc.SetDacl(dacl);
    a_secAttrs.Set(desc);
}

//************************************************************************
BOOL TSharedMemoryPipe::Create(
                   const ATL::CSid & a_sid,
                   DWORD   a_dwKey, 
                   long    a_nExtraHandleCount,
                   HANDLE *a_phExtraHandles)
{
    m_sid = a_sid;

    ATL::CSecurityAttributes secAttrs;
    InitSecurityAttributes(secAttrs);

    LPC_OBJECT_NAMETAB  objNames = {0};
    PIPE_BUFFER        *pBuffs = NULL;

    GetObjectNames(a_dwKey, true, &objNames);

    m_hSemWrite  = CreateSemaphore(&secAttrs, 1, 10, objNames.m_szSemOwnWrite);
    m_hSemRead   = CreateSemaphore(&secAttrs, 0, 10, objNames.m_szSemOwnRead);
    m_hSemWrite2 = CreateSemaphore(&secAttrs, 1, 10, objNames.m_szSemPeerWrite);
    m_hSemRead2  = CreateSemaphore(&secAttrs, 0, 10, objNames.m_szSemPeerRead);
    m_hShared    = CreateFileMapping((HANDLE)-1, &secAttrs, PAGE_READWRITE, 0, 2*sizeof(PIPE_BUFFER), objNames.m_szSharedMemName);
    m_hDiscMutex = CreateMutex(&secAttrs, FALSE, objNames.m_szDiscMutexOwnName);
    m_hDiscMutex2 = CreateMutex(&secAttrs, FALSE, objNames.m_szDiscMutexPeerName);
    m_hReadyEvent  = CreateEvent(&secAttrs, FALSE, FALSE, objNames.m_szOwnReadyEventName);
    m_hReadyEvent2 = CreateEvent(&secAttrs, FALSE, FALSE, objNames.m_szPeerReadyEventName);

    if (!m_hSemWrite || !m_hSemRead || !m_hSemWrite2 || !m_hSemRead2 || !m_hShared || 
        !m_hDiscMutex || !m_hDiscMutex2 || !m_hReadyEvent || !m_hReadyEvent2)
    {
        CloseHandles();
        return FALSE;
    }
    m_pvShared = MapViewOfFile(m_hShared, FILE_MAP_WRITE, 0, 0, 2*sizeof(PIPE_BUFFER));
    if (!m_pvShared)
    {
        CloseHandles();
        return FALSE;
    }
    pBuffs = (PIPE_BUFFER*)m_pvShared;
    m_pBuffSend    = &pBuffs[0];
    m_pBuffReceive = &pBuffs[1];

    if(a_nExtraHandleCount > 0)
    {
        assert(a_nExtraHandleCount < 10);
        m_nExtraHandleCount = a_nExtraHandleCount;
        memcpy(m_hExtraHandles, a_phExtraHandles, m_nExtraHandleCount * sizeof(HANDLE));
    }
    return TRUE;
}

//************************************************************************
BOOL TSharedMemoryPipe::Open(
                 DWORD   a_dwKey, 
                 long    a_nExtraHandleCount,
                 HANDLE *a_phExtraHandles)
{
    LPC_OBJECT_NAMETAB  objNames = {0};
    PIPE_BUFFER        *pBuffs = NULL;

    GetObjectNames(a_dwKey, false, &objNames);

    m_hSemWrite   = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, objNames.m_szSemOwnWrite);
    m_hSemRead    = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, objNames.m_szSemOwnRead);
    m_hSemWrite2  = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, objNames.m_szSemPeerWrite);
    m_hSemRead2   = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, objNames.m_szSemPeerRead);
    m_hShared     = OpenFileMapping(FILE_MAP_WRITE, FALSE, objNames.m_szSharedMemName);
    m_hDiscMutex  = OpenMutex(MUTEX_ALL_ACCESS, FALSE, objNames.m_szDiscMutexOwnName);
    m_hDiscMutex2 = OpenMutex(MUTEX_ALL_ACCESS, FALSE, objNames.m_szDiscMutexPeerName);
    m_hReadyEvent  = OpenEvent(EVENT_MODIFY_STATE, FALSE, objNames.m_szOwnReadyEventName);
    m_hReadyEvent2 = OpenEvent(EVENT_ALL_ACCESS,   FALSE, objNames.m_szPeerReadyEventName);

    if (!m_hSemWrite || !m_hSemRead || !m_hSemWrite2 || !m_hSemRead2 || !m_hShared || 
        !m_hDiscMutex || !m_hDiscMutex2 || !m_hReadyEvent || !m_hReadyEvent2)
    {
        CloseHandles();
        return FALSE;
    }
    m_pvShared = MapViewOfFile(m_hShared, FILE_MAP_WRITE, 0, 0, 2*sizeof(PIPE_BUFFER));
    if (!m_pvShared)
    {
        CloseHandles();
        return FALSE;
    }
    pBuffs = (PIPE_BUFFER*)m_pvShared;
    m_pBuffSend    = &pBuffs[1];
    m_pBuffReceive = &pBuffs[0];

    if(a_nExtraHandleCount > 0)
    {
        assert(a_nExtraHandleCount < 10);
        m_nExtraHandleCount = a_nExtraHandleCount;
        memcpy(m_hExtraHandles, a_phExtraHandles, m_nExtraHandleCount * sizeof(HANDLE));
    }

    return TRUE;
}


//************************************************************************
void TSharedMemoryPipe::CloseHandles()
{
    _S_CloseHandle(m_hSemRead);
    _S_CloseHandle(m_hSemWrite);
    _S_CloseHandle(m_hSemRead2);
    _S_CloseHandle(m_hSemWrite2);
    _S_CloseHandle(m_hShared);
    _S_CloseHandle(m_hDiscMutex);
    _S_CloseHandle(m_hDiscMutex2);
    _S_CloseHandle(m_hReadyEvent);
    _S_CloseHandle(m_hReadyEvent2);
}

//************************************************************************
void TSharedMemoryPipe::WaitForObject(HANDLE a_phEvent)
{
    HANDLE  hObjects[12] = {0};
    DWORD   dwWaitRes = 0;

    hObjects[0] = a_phEvent;
    hObjects[1] = m_hDiscMutex;

    if (m_nExtraHandleCount > 0)
        memcpy(&hObjects[2], m_hExtraHandles, sizeof(HANDLE)*(m_nExtraHandleCount));

    dwWaitRes = WaitForMultipleObjects(m_nExtraHandleCount + 2, hObjects, FALSE, INFINITE);

    if (dwWaitRes != WAIT_OBJECT_0)
    {
        m_Exception.m_nInterruptHandle = dwWaitRes - WAIT_OBJECT_0 - 2;
        throw &m_Exception;
    }
}

//************************************************************************
void TSharedMemoryPipe::Commit()
{
    LONG nPrevSem = 0;
    WaitForObject(m_hSemWrite);

    memcpy(m_pBuffSend->m_data, m_buffOut.m_data, m_buffOut.m_nCount); 
    m_pBuffSend->m_nCount = m_buffOut.m_nCount;
    m_buffOut.m_nCount    = 0;

    ReleaseSemaphore(m_hSemRead2, 1, &nPrevSem);
}

//************************************************************************
void TSharedMemoryPipe::Acquire()
{
    LONG nPrevSem = 0;
    WaitForObject(m_hSemRead);

    memcpy(m_buffIn.m_data, m_pBuffReceive->m_data, m_pBuffReceive->m_nCount); 
    m_buffIn.m_nCount        = m_pBuffReceive->m_nCount;
    m_pBuffReceive->m_nCount = 0;
    m_nReceiveOfs            = 0;

    ReleaseSemaphore(m_hSemWrite2, 1, &nPrevSem);
}

//************************************************************************
void TSharedMemoryPipe::Write(LPVOID a_pvBuff, long a_cbSize)
{
    assert(m_bReady);

    BYTE *pbyBuff = (BYTE*)a_pvBuff;
    long  cbFree  = 0;
    long  cbWritten = 0;

    do
    {
        cbFree  = PIPE_BUFFER_SIZE - m_buffOut.m_nCount;
        if (cbFree>0)
        {
            cbWritten = min(cbFree, a_cbSize);
            memcpy(&m_buffOut.m_data[m_buffOut.m_nCount], pbyBuff, cbWritten);

            a_cbSize -= cbWritten;
            m_buffOut.m_nCount += cbWritten;
            pbyBuff += cbWritten;

        }
        if (a_cbSize==0)
            return;
        Commit();
    }while(1);
}

//************************************************************************
void TSharedMemoryPipe::Read(LPVOID a_pvBuff, long a_cbSize)
{
    assert(m_bReady);

    BYTE *pbyBuff = (BYTE*)a_pvBuff;
    long  cbData  = 0;
    long  cbRead  = 0;

    do
    {
        cbData = m_buffIn.m_nCount - m_nReceiveOfs;
        if (cbData>0)
        {
            cbRead = min(cbData, a_cbSize);
            memcpy(pbyBuff, &m_buffIn.m_data[m_nReceiveOfs], cbRead);

            a_cbSize -= cbRead;
            m_nReceiveOfs += cbRead;
            pbyBuff += cbRead;

        }
        if (a_cbSize==0)
            return;
        Acquire();
    }while(1);
}

//************************************************************************
void TSharedMemoryPipe::AddWaitObject(HANDLE a_hObject)
{
    m_hExtraHandles[m_nExtraHandleCount++] = a_hObject;
}

//************************************************************************
ATL::CSid TSharedMemoryPipe::GetClientUserId()
{
    return m_sid;
}

//************************************************************************
void TSharedMemoryPipe::Close()
{
    delete this;
}

//************************************************************************
TPipe* TSharedMemoryPipeFactory::OpenAtClient()
{
    TSharedMemoryPipe* pPipe = new TSharedMemoryPipe(m_dwThreadID);
    if (!pPipe->Open(m_dwThreadID))
    {
        delete pPipe;
        return FALSE;
    }

    return pPipe;
}

//************************************************************************
TPipe* TSharedMemoryPipeFactory::CreateAtServer()
{
    TSharedMemoryPipe* pPipe = new TSharedMemoryPipe(m_dwThreadID);
    if(pPipe->Create(m_sid, m_dwThreadID))
    {
        return pPipe;
    }

    delete pPipe;
    return FALSE;
}

//************************************************************************
bool TSharedMemoryPipe::SyncBarrier()
{
    assert( !m_bReady );

    // own disconnect mutex
    WaitForSingleObject(m_hDiscMutex2, INFINITE);

    SetEvent(m_hReadyEvent);

    m_bReady = (WaitForSingleObject(m_hReadyEvent2, LPC_TIMEOUT) == WAIT_OBJECT_0);
    return m_bReady;
}

//************************************************************************
HANDLE TSharedMemoryPipe::CreateSharedMemoryForBlob(bool a_bServer)
{
    LPC_OBJECT_NAME xBlobBufName={0};

    COMMON_GetObjectNames(m_dwThreadID, NULL, &xBlobBufName);
    if(a_bServer)
    {
        ATL::CSecurityAttributes secAttrs;
        InitSecurityAttributes(secAttrs);
        return CreateFileMapping(INVALID_HANDLE_VALUE, &secAttrs, PAGE_READWRITE|SEC_RESERVE, 0, BLOB_SIZE_MAX, xBlobBufName);
    }
    else
        return OpenFileMapping(FILE_MAP_WRITE, FALSE, xBlobBufName);
}

//************************************************************************
TSharedMemoryPipeFactory::~TSharedMemoryPipeFactory()
{
}
