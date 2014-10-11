#pragma once
#include "lpc.h"
#include <atlsecurity.h>

class TSharedMemoryPipe : public TPipe
{
    HANDLE m_hSemWrite,  m_hSemRead;
    HANDLE m_hSemWrite2, m_hSemRead2;
    HANDLE m_hShared, m_hDiscMutex, m_hDiscMutex2;
    HANDLE m_hReadyEvent, m_hReadyEvent2;
    LPVOID m_pvShared;
    HANDLE  m_hExtraHandles[10];
    long    m_nExtraHandleCount;
    DWORD   m_dwThreadID;
    ATL::CSid m_sid;
    bool      m_bReady;

    PIPE_BUFFER m_buffIn, m_buffOut;
    PIPE_BUFFER *m_pBuffSend, *m_pBuffReceive; 
    long        m_nSendOfs, m_nReceiveOfs;  
public:
    virtual void AddWaitObject(HANDLE a_hObject);
	virtual void Write(LPVOID a_pvBuff, long a_cbSize);
	virtual void Read(LPVOID a_pvBuff, long a_cbSize);
    virtual void Close();
    virtual bool SyncBarrier();
    virtual ATL::CSid GetClientUserId();
    virtual HANDLE CreateSharedMemoryForBlob(bool a_bServer);
    TPipeException m_Exception;

    TSharedMemoryPipe(DWORD a_dwThreadID);
    ~TSharedMemoryPipe();
    BOOL Create(const ATL::CSid & a_sid,
                DWORD   a_dwKey, 
                long    a_nExtraHandleCount = 0,
                HANDLE *a_phExtraHandles = NULL);
    BOOL Open(DWORD   a_dwKey, 
              long    a_nExtraHandleCount = 0,
              HANDLE *a_phExtraHandles = NULL);
    void Commit();
    void Acquire();
private:
    void InitSecurityAttributes(ATL::CSecurityAttributes &a_secAttrs);
	void CloseHandles();
    void GetObjectNames(DWORD a_dwKey, bool a_bServer, LPC_OBJECT_NAMETAB *a_pObjectNames);
	void WaitForObject(HANDLE a_phEvent);
};


class TSharedMemoryPipeFactory: public TPipeFactory
{
    DWORD     m_dwThreadID;
    ATL::CSid m_sid;
public:
    inline TSharedMemoryPipeFactory(DWORD a_dwThreadID, const ATL::CSid & a_sid): 
        m_dwThreadID(a_dwThreadID), m_sid(a_sid)
    {}
    ~TSharedMemoryPipeFactory();

	virtual TPipe* OpenAtClient();
	virtual TPipe* CreateAtServer();
};