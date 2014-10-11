// File lpc.h

#if !defined(LPC_H)
#define LPC_H

#include "TreeBaseCommon.h"
#include "TreeBase.h"
#include <atlsecurity.h>

#define CP_NONE          0
#define CP_LONG          1
#define CP_TEXT          2
#define CP_TEXTPOINTER   3
#define CP_BUFFER        4
#define CP_BUFFERPOINTER 5
#define CP_HANDLE        6

#define PIPE_BUFFER_SIZE 8192
#define CP_PARAMCOUNT    17

#define LPC_TIMEOUT 100000

typedef struct
{
    long  m_nCount;
    BYTE  m_data[PIPE_BUFFER_SIZE];
}PIPE_BUFFER;

typedef char LPC_OBJECT_NAME[256];

typedef struct
{
    LPC_OBJECT_NAME m_szSharedMemName;
    LPC_OBJECT_NAME m_szSemOwnWrite;
    LPC_OBJECT_NAME m_szSemOwnRead;
    LPC_OBJECT_NAME m_szOwnReadyEventName;
    LPC_OBJECT_NAME m_szDiscMutexOwnName;
    LPC_OBJECT_NAME m_szSemPeerWrite;
    LPC_OBJECT_NAME m_szSemPeerRead;
    LPC_OBJECT_NAME m_szPeerReadyEventName;
    LPC_OBJECT_NAME m_szDiscMutexPeerName;
}LPC_OBJECT_NAMETAB;

typedef struct
{
    DWORD dwThreadId;
    BOOL  bResult;
    BYTE  clientSid[SECURITY_MAX_SID_SIZE];
    LPC_OBJECT_NAME szSignature;
}SERVER_ENTRY_BLOCK;

struct TPipeException
{
    long m_nInterruptHandle;
};

typedef struct
{
    long nID;
    long nType;
    long cbSize;
    union
    {
        long     nLong;
        HTBHANDLE handle;
        LPVOID   pvBuff;
        LPSTR    pszText;
    };
}CALL_PARAM;

class TPipe;

void _S_CloseHandle(HANDLE &a_handle);


class __TBCOMMON_EXPORT__ TCallParams
{
    CALL_PARAM m_params[CP_PARAMCOUNT];
    long       m_nCount; 

    long GetIndexOfID(long a_nID);
    long GetFreeIndexOfID(long a_nID);
public:
    long m_nFunction;
    TCallParams(long a_nFunction = 0);
    ~TCallParams();

    void SetTextPointer(long a_nID, LPCSTR a_pszText);
    void SetText(long a_nID, LPCSTR a_pszText);
    void EatBuffer(long a_nID, LPVOID a_pvBuffer);
    void SetBuffer(long a_nID, LPVOID a_pvBuffer, long a_cbSize);
    void SetBufferPointer(long a_nID, LPVOID a_pvBuffer, long a_cbSize);
    void SetHandle(long a_nID, HTBHANDLE a_handle);

    BOOL GetBuffer(long a_nID, LPVOID *a_ppvBuff, long &a_cbSize);
    BOOL ExtractBuffer(long a_nID, LPVOID *a_ppvBuff, long &a_cbSize);

    LPCSTR GetText(long a_nID);
    long GetLong(long a_nID);
    HTBHANDLE GetHandle(long a_nID);

public:
	void Write(TPipe *a_pipe);
	void Read(TPipe *a_pipe);
	void Reset();
	void SetLong(long a_nID, long a_nLong);
};


class __TBCOMMON_EXPORT__ TPipeFactory
{
public:
    static TPipeFactory* GetInstanceForSharedMemory(DWORD a_dwThreadId, const ATL::CSid & a_sid);

	virtual TPipe* OpenAtClient() = 0;
	virtual TPipe* CreateAtServer() = 0;
    virtual ~TPipeFactory(){};
};  


class __TBCOMMON_EXPORT__ TPipe
{
public:
    virtual HANDLE CreateSharedMemoryForBlob(bool a_bServer) = 0;
	virtual void Write(LPVOID a_pvBuff, long a_cbSize) = 0;
	virtual void Read(LPVOID a_pvBuff, long a_cbSize) = 0;
    virtual void Commit() = 0;
    virtual void Close() = 0;
    virtual bool SyncBarrier() = 0;
    virtual void AddWaitObject(HANDLE a_hObject) = 0;
    virtual ATL::CSid GetClientUserId() = 0;
};  

class __TBCOMMON_EXPORT__ TServerEntry
{
    static LPCSTR S_szInitEvent;
    static LPCSTR S_szInitEventAdmin;
    static LPCSTR S_szReplyEvent;
    static LPCSTR S_szMutex;
    static LPCSTR S_szSharedMem;

    HANDLE m_hMutex;
    HANDLE m_hSharedMem;

    SERVER_ENTRY_BLOCK *m_pBlock;
    void CloseHandles();
public:
    HANDLE m_hInitEvent, m_hInitEventAdmin;
    HANDLE m_hReplyEvent;

    TServerEntry();
    ~TServerEntry();
    BOOL Create(LPSECURITY_ATTRIBUTES a_SecAttrs, LPSECURITY_ATTRIBUTES a_adminSecAttrs);
    BOOL Open();

    SERVER_ENTRY_BLOCK *GetBlock();
    void                ReleaseBlock();

    inline BOOL Lock()
    {
        return WaitForSingleObject(m_hMutex, INFINITE)==WAIT_OBJECT_0;
    }
    inline void Unlock()
    {
        ReleaseMutex(m_hMutex);
    }
    inline BOOL IsOpen()
    {
        return m_hSharedMem!=NULL;
    }
};


void __TBCOMMON_EXPORT__ COMMON_GetObjectNames(DWORD a_dwKey, LPC_OBJECT_NAME *a_pszEvent, LPC_OBJECT_NAME *a_pszBlobBuf);


#endif //LPC_H