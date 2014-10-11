// calls.h
#if !defined(_CALLS_H_)
#define _CALLS_H_

#include "TTask.h"
#include "SecurityFacade.h"
#include "lpc.h"
#include <afxtempl.h>

class TClientTask: public TTask
{
    friend CString TASK_GetComputerName();

    TPipe    *m_pPipe;
    HANDLE    m_hBlobBuf;
    DWORD     m_dwClientThreadId;
    bool      m_bAuthenticated, m_bAdmin;
    CString   m_strComputer;
    ATL::CSid m_sid;
public:
    static CMap<DWORD, DWORD, DWORD, DWORD> S_mapClientThreads; 
    static CRITICAL_SECTION                 S_cs;
public:
    TClientTask(TPipe *a_pPipe, bool a_bAdmin): 
        m_pPipe(a_pPipe), 
        m_hBlobBuf(NULL),
        m_bAuthenticated(false),
        m_bAdmin(a_bAdmin)
    {
    }
    ~TClientTask();
    virtual BOOL OnStart();
    virtual void OnRun();
    virtual void OnFinish(); 
    virtual void OnBeforeRun();

    void SetClientInfo(LPCSTR pszClientExe, DWORD dwClientThread, LPCSTR pszComputer, LPCSTR pszUser);
private:
	void CallServer(TCallParams &a_inParams, TCallParams &a_outParams);
};

#define BEGIN_CALLSERVER_DISPATCH()\
static BOOL _CallServerDispatch(TCallParams &a_inParams, TCallParams &a_outParams)\
{\
    switch (a_inParams.m_nFunction)\
    {\

#define CSD_(id, proc)\
        case id:\
            return proc(a_inParams, a_outParams);\

#define CSD2_(id, proc, a1, a2)\
        case id:\
            return proc<a1,a2>(a_inParams, a_outParams);\

#define END_CALLSERVER_DISPATCH()\
    }\
    return FALSE;\
}

#endif //_CALLS_H_