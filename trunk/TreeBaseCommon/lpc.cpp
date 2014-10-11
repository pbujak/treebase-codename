#include "stdafx.h"
#include "lpc.h"
#include "TreeBasePriv.h"
#include "TreeBaseInt.h"
#include "TSharedMemoryPipe.h"

//************************************************************************
void _S_CloseHandle(HANDLE &a_handle)
{
    if (a_handle!=NULL)
    {
        CloseHandle(a_handle);
        a_handle = NULL;
    }
}

//=======================================================================
LPCSTR TServerEntry::S_szInitEvent      = KOBJNAME("TreeBase:Server:InitEvent");
LPCSTR TServerEntry::S_szInitEventAdmin = KOBJNAME("TreeBase:Server:InitEventAdmin");
LPCSTR TServerEntry::S_szReplyEvent     = KOBJNAME("TreeBase:Server:ReplyEvent");
LPCSTR TServerEntry::S_szMutex          = KOBJNAME("TreeBase:Server:Mutex");
LPCSTR TServerEntry::S_szSharedMem      = KOBJNAME("TreeBase:Server:SharedMem");

//************************************************************************
TServerEntry::TServerEntry()
{
    m_hInitEventAdmin = NULL;
    m_hInitEvent  = NULL;
    m_hReplyEvent = NULL;
    m_hMutex      = NULL;
    m_hSharedMem  = NULL;
    m_pBlock      = NULL;
}

//************************************************************************
TServerEntry::~TServerEntry()
{
    CloseHandles();
}

//************************************************************************
void TServerEntry::CloseHandles()
{
    ReleaseBlock();
    _S_CloseHandle(m_hInitEvent);
    _S_CloseHandle(m_hInitEventAdmin);
    _S_CloseHandle(m_hReplyEvent);
    _S_CloseHandle(m_hMutex);
    _S_CloseHandle(m_hSharedMem);
}

//************************************************************************
BOOL TServerEntry::Create(LPSECURITY_ATTRIBUTES a_SecAttrs, LPSECURITY_ATTRIBUTES a_adminSecAttrs)
{
    SERVER_ENTRY_BLOCK *pBlock = NULL;
    BOOL                bOK    = FALSE;

    m_hInitEvent      = CreateEvent(a_SecAttrs, FALSE, FALSE, S_szInitEvent);
    m_hInitEventAdmin = CreateEvent(a_adminSecAttrs, FALSE, FALSE, S_szInitEventAdmin);
    m_hReplyEvent     = CreateEvent(a_SecAttrs, FALSE, FALSE, S_szReplyEvent);
    m_hMutex          = CreateMutex(a_SecAttrs, FALSE, S_szMutex);
    m_hSharedMem      = CreateFileMapping(INVALID_HANDLE_VALUE, a_SecAttrs, PAGE_READWRITE, 0, sizeof(SERVER_ENTRY_BLOCK), S_szSharedMem);

    if (m_hInitEvent==NULL || m_hInitEventAdmin==NULL || m_hReplyEvent==NULL || m_hMutex==NULL || m_hSharedMem==NULL)
    {
        CloseHandles();
        return FALSE;
    }
    pBlock = GetBlock();
    if(pBlock)
    {
        bOK = strcmp(pBlock->szSignature, S_szSharedMem)!=0; //not equal
        strcpy(pBlock->szSignature, S_szSharedMem);
        ReleaseBlock();
    }
    return bOK;
}

//************************************************************************
BOOL TServerEntry::Open()
{
    m_hInitEvent  = OpenEvent(EVENT_ALL_ACCESS, FALSE, S_szInitEventAdmin);
    if(m_hInitEvent == NULL)
        m_hInitEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, S_szInitEvent);

    m_hReplyEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, S_szReplyEvent);
    m_hMutex      = OpenMutex(MUTEX_ALL_ACCESS,   FALSE, S_szMutex);
    m_hSharedMem  = OpenFileMapping(FILE_MAP_WRITE, FALSE, S_szSharedMem);

    if (m_hInitEvent==NULL || m_hReplyEvent==NULL || m_hMutex==NULL || m_hSharedMem==NULL)
    {
        CloseHandles();
        return FALSE;
    }
    return TRUE;
}

//************************************************************************
SERVER_ENTRY_BLOCK *TServerEntry::GetBlock()
{
    if (m_hSharedMem==NULL)
        return NULL;

    if(m_pBlock==NULL)
    {
        m_pBlock = (SERVER_ENTRY_BLOCK *)MapViewOfFile(m_hSharedMem, FILE_MAP_WRITE, 0, 0, sizeof(SERVER_ENTRY_BLOCK));
    }
    return m_pBlock;
};

//************************************************************************
void TServerEntry::ReleaseBlock()
{
    if(m_pBlock!=NULL)
    {
        UnmapViewOfFile(m_pBlock);
        m_pBlock = NULL;
    }
};



//************************************************************************
LPCSTR __TBCOMMON_EXPORT__ COMMON_GetEntryPointName()
{
    static char szName[] = "TreeBase:Server:EntryPoint";
    return szName;
}

//************************************************************************
LPCSTR __TBCOMMON_EXPORT__ COMMON_GetGlobalMutexName()
{
    static char szName[] = KOBJNAME("TreeBase Server Global Control Mutex");
    return szName;
}


//************************************************************************
void __TBCOMMON_EXPORT__ COMMON_GetObjectNames(DWORD a_dwKey, LPC_OBJECT_NAME *a_pszEvent, LPC_OBJECT_NAME *a_pszBlobBuf)
{
    if(a_pszEvent)
        wsprintf(*a_pszEvent,   KOBJNAME("TreeBase Server LPC ReadyEvent: %X"), a_dwKey);

    if(a_pszBlobBuf)
        wsprintf(*a_pszBlobBuf, KOBJNAME("TreeBase Server LPC BlobBuffer: %X"), a_dwKey);
}


//************************************************************************
inline static void WriteLong(TPipe *a_pipe, long a_long)
{
    a_pipe->Write(&a_long, sizeof(a_long));
}

//************************************************************************
inline static long ReadLong(TPipe *a_pipe)
{
    long nLong;
    a_pipe->Read(&nLong, sizeof(nLong));
    return nLong;
}

//************************************************************************
static void WriteHandle(TPipe *a_pipe, HTBHANDLE a_handle)
{
    a_pipe->Write(&a_handle, sizeof(a_handle));
}

//************************************************************************
static HTBHANDLE ReadHandle(TPipe *a_pipe)
{
    HTBHANDLE handle = NULL;
    a_pipe->Read(&handle, sizeof(handle));
    return handle;
}

//************************************************************************
static void WriteString(TPipe *a_pipe, LPCSTR a_pszString)
{
    long nLen = a_pszString ? strlen(a_pszString)+1 : 0;

    WriteLong(a_pipe, nLen);
    if (nLen>0)
        a_pipe->Write((LPVOID)a_pszString, nLen);
}

//************************************************************************
static LPSTR ReadString(TPipe *a_pipe)
{
    long nLen = 0;
    LPSTR pszText = NULL;

    nLen = ReadLong(a_pipe);
    if (nLen==0)
        return NULL;
    pszText = (LPSTR)SYS_MemAlloc(nLen);
    try{
        a_pipe->Read(pszText, nLen);
    }
    catch(TPipeException *)
    {
        SYS_MemFree(pszText);
        throw;
    }
    return pszText;
}

//************************************************************************
static void WriteBlock(TPipe *a_pipe, LPVOID a_pvBuff, long a_cbSize)
{
    WriteLong(a_pipe, a_cbSize);
    a_pipe->Write(a_pvBuff, a_cbSize);
}

//************************************************************************
static void ReadBlock(TPipe *a_pipe, LPVOID *a_ppvBuff, long &a_cbSize)
{
    long cbSize = 0;
    LPVOID pvBuff = NULL;

    cbSize = ReadLong(a_pipe);
    pvBuff = SYS_MemAlloc(cbSize);
    try{
        a_pipe->Read(pvBuff, cbSize);
    }
    catch(TPipeException *)
    {
        SYS_MemFree(pvBuff);
        throw;
    }
    *a_ppvBuff = pvBuff;
    a_cbSize = cbSize;
}


//************************************************************************
TCallParams::TCallParams(long a_nFunction)
{
    memset(m_params, 0, sizeof(m_params));
    m_nCount = 0;
    m_nFunction = a_nFunction;
}

//************************************************************************
TCallParams::~TCallParams()
{
    Reset();
}

//************************************************************************
void TCallParams::SetLong(long a_nID, long a_nLong)
{
    long nIndex = GetFreeIndexOfID(a_nID);

    if (nIndex==-1)
        return;
    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_LONG;
    m_params[nIndex].nLong = a_nLong;
    m_nCount++;
}

//************************************************************************
void TCallParams::SetHandle(long a_nID, HTBHANDLE a_handle)
{
    long nIndex = GetFreeIndexOfID(a_nID);

    if (nIndex==-1)
        return;
    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_HANDLE;
    m_params[nIndex].handle = a_handle;
    m_nCount++;
}

//************************************************************************
void TCallParams::SetText(long a_nID, LPCSTR a_pszText)
{
    long  nIndex = GetFreeIndexOfID(a_nID);
    long  cbSize = strlen(a_pszText)+1;
    LPSTR pszText = NULL;

    if (nIndex==-1)
        return;

    pszText = (LPSTR)SYS_MemAlloc(cbSize);
    memcpy(pszText, a_pszText, cbSize);

    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_TEXT;
    m_params[nIndex].pszText = pszText;
    m_params[nIndex].cbSize  = cbSize;
    m_nCount++;
}

//************************************************************************
void TCallParams::SetTextPointer(long a_nID, LPCSTR a_pszText)
{
    long nIndex = GetFreeIndexOfID(a_nID);

    if (nIndex==-1)
        return;
    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_TEXTPOINTER;
    m_params[nIndex].pszText = (LPSTR)a_pszText;
    m_params[nIndex].cbSize  = strlen(a_pszText)+1;
    m_nCount++;
}

//************************************************************************
void TCallParams::SetBuffer(long a_nID, LPVOID a_pvBuffer, long a_cbSize)
{
    long  nIndex = GetFreeIndexOfID(a_nID);
    LPVOID pvBuff = NULL;

    if (nIndex==-1)
        return;

    pvBuff = SYS_MemAlloc(a_cbSize);
    memcpy(pvBuff, a_pvBuffer, a_cbSize);

    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_BUFFER;
    m_params[nIndex].pvBuff = pvBuff;
    m_params[nIndex].cbSize  = a_cbSize;
    m_nCount++;
};

//************************************************************************
void TCallParams::SetBufferPointer(long a_nID, LPVOID a_pvBuffer, long a_cbSize)
{
    long  nIndex = GetFreeIndexOfID(a_nID);

    if (nIndex==-1)
        return;

    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_BUFFERPOINTER;
    m_params[nIndex].pvBuff = a_pvBuffer;
    m_params[nIndex].cbSize  = a_cbSize;
    m_nCount++;
};

//************************************************************************
void TCallParams::EatBuffer(long a_nID, LPVOID a_pvBuffer)
{
    long  nIndex = GetFreeIndexOfID(a_nID);

    if (nIndex==-1 || a_pvBuffer==NULL)
        return;

    m_params[nIndex].nID   = a_nID;
    m_params[nIndex].nType = CP_BUFFER;
    m_params[nIndex].pvBuff = a_pvBuffer;
    m_params[nIndex].cbSize  = SYS_MemSize(a_pvBuffer);
    m_nCount++;
};


//************************************************************************
long TCallParams::GetLong(long a_nID)
{
    long nIndex = GetIndexOfID(a_nID);

    if (nIndex!=-1 && m_params[nIndex].nType == CP_LONG)
        return m_params[nIndex].nLong;
    return -1;
}

//************************************************************************
HTBHANDLE TCallParams::GetHandle(long a_nID)
{
    long nIndex = GetIndexOfID(a_nID);

    if (nIndex!=-1 && m_params[nIndex].nType == CP_HANDLE)
        return m_params[nIndex].handle;
    return NULL;
}

//************************************************************************
LPCSTR TCallParams::GetText(long a_nID)
{
    long nIndex = GetIndexOfID(a_nID);

    if (nIndex!=-1 && (m_params[nIndex].nType == CP_TEXT || m_params[nIndex].nType == CP_TEXTPOINTER))
        return m_params[nIndex].pszText;
    return NULL;
}

//************************************************************************
BOOL TCallParams::GetBuffer(long a_nID, LPVOID *a_ppvBuff, long &a_cbSize)
{
    long nIndex = GetIndexOfID(a_nID);

    if (nIndex!=-1 && (m_params[nIndex].nType == CP_BUFFER || m_params[nIndex].nType == CP_BUFFERPOINTER))
    {
        *a_ppvBuff = m_params[nIndex].pvBuff;
        a_cbSize = m_params[nIndex].cbSize;
        return TRUE;
    }
    return FALSE;
}

//************************************************************************
BOOL TCallParams::ExtractBuffer(long a_nID, LPVOID *a_ppvBuff, long &a_cbSize)
{
    long nIndex = GetIndexOfID(a_nID);

    if (nIndex!=-1 && (m_params[nIndex].nType == CP_BUFFER || m_params[nIndex].nType == CP_BUFFERPOINTER))
    {
        *a_ppvBuff = m_params[nIndex].pvBuff;
        a_cbSize = m_params[nIndex].cbSize;

        m_params[nIndex].pvBuff = NULL;
        m_params[nIndex].nType  = CP_NONE;
        return TRUE;
    }
    return FALSE;
}


//************************************************************************
long TCallParams::GetFreeIndexOfID(long a_nID)
{
    long nStart = a_nID % CP_PARAMCOUNT;
    long nPos   = 0;
    long ctr  = 0;

    for (ctr=nStart; ctr<nPos+CP_PARAMCOUNT; ctr++)
    {
        nPos = ctr % CP_PARAMCOUNT;
        if (m_params[nPos].nType==CP_NONE)
            return nPos;
    }
    return -1;
}

//************************************************************************
long TCallParams::GetIndexOfID(long a_nID)
{
    long nStart = a_nID % CP_PARAMCOUNT;
    long nPos   = 0;
    long ctr  = 0;

    for (ctr=nStart; ctr<nPos+CP_PARAMCOUNT; ctr++)
    {
        nPos = ctr % CP_PARAMCOUNT;
        if (m_params[nPos].nID==a_nID)
            return nPos;
    }
    return -1;
}

//************************************************************************
void TCallParams::Reset()
{
    long ctr = 0;

    for (ctr=0; ctr<CP_PARAMCOUNT; ctr++)
    {
        if (m_params[ctr].nType==CP_BUFFER)
            SYS_MemFree(m_params[ctr].pvBuff);
        if (m_params[ctr].nType==CP_TEXT && m_params[ctr].pszText!=NULL)
            SYS_MemFree(m_params[ctr].pszText);
    }
    memset(m_params, 0, sizeof(m_params));
    m_nCount = 0;
}

//************************************************************************
void TCallParams::Write(TPipe *a_pipe)
{
    long nIdx = 0; 
    long ctr = 0;
    long nCount = 0;
    long nType  = 0;

    WriteLong(a_pipe, m_nFunction);
    WriteLong(a_pipe, m_nCount);

    for (ctr=0; ctr<CP_PARAMCOUNT; ctr++)
    {
        nType = m_params[ctr].nType;
        if (nType!=CP_NONE)
        {
            WriteLong(a_pipe, ctr);
            WriteLong(a_pipe, m_params[ctr].nID);

            if (nType==CP_TEXTPOINTER)
                nType = CP_TEXT;
            if (nType==CP_BUFFERPOINTER)
                nType = CP_BUFFER;
            WriteLong(a_pipe, nType);

            if (nType==CP_LONG)
                WriteLong(a_pipe, m_params[ctr].nLong);
            if (nType==CP_HANDLE)
                WriteHandle(a_pipe, m_params[ctr].handle);
            if (nType==CP_TEXT)
                WriteString(a_pipe, m_params[ctr].pszText);
            if (nType==CP_BUFFER)
                WriteBlock(a_pipe, m_params[ctr].pvBuff, m_params[ctr].cbSize);
        }
    }
}

//************************************************************************
void TCallParams::Read(TPipe *a_pipe)
{
    CALL_PARAM  param = {0};
    long        nCount = 0;
    long        ctr=0,nIdx=0;
    LPSTR       pszText = NULL;
    LPVOID      pvBuff = NULL;
    long        cbSize = 0;

    Reset();

    m_nFunction = ReadLong(a_pipe);
    nCount = ReadLong(a_pipe);

    for (ctr=0; ctr<nCount; ctr++)
    {
        nIdx  = ReadLong(a_pipe);
        param.nID   = ReadLong(a_pipe);
        param.nType = ReadLong(a_pipe);

        if (param.nType==CP_LONG)
            param.nLong = ReadLong(a_pipe);
        if (param.nType==CP_HANDLE)
            param.handle = ReadHandle(a_pipe);
        if (param.nType==CP_TEXT)
            param.pszText = ReadString(a_pipe);
        if (param.nType==CP_BUFFER)
            ReadBlock(a_pipe, &param.pvBuff, param.cbSize);
        m_params[nIdx] = param;
        m_nCount++;
    }

}

//************************************************************************
TPipeFactory* TPipeFactory::GetInstanceForSharedMemory(DWORD a_dwThreadId, const ATL::CSid & a_sid)
{
    return new TSharedMemoryPipeFactory(a_dwThreadId, a_sid);
}
