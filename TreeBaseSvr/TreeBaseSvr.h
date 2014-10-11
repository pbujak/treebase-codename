#ifndef _TREE_BASE_SVR_MAIN_
#define _TREE_BASE_SVR_MAIN_

#include "Actor.h"
#include <assert.h>
#include <atlsecurity.h>

#pragma warning(disable:4355)

struct DBIDS
{
    signed mainDB: 8;
    signed userDB: 8;
    signed dbDB: 8;
};
struct SEGID_F;

class TCompactTask;
class TSection;
//==================================================================
class TRunningContext
{
public:
    enum EState
    {
        eStart = 1, eRunning, eStopping, eStopped, eShutDownReady
    };
public:
    virtual void error(LPCSTR a_pszError) = 0;
    virtual void setState(EState a_state) {};
    virtual void enableStop(bool a_bEnable) {};
};

//==================================================================
class TServerMainTask: public Actor::TMessageQueueTask
{
    typedef Actor::TMessageFactory1<TServerMainTask, bool> TMFClientConnection;
    
    TMFClientConnection m_mfClientConnection, m_mfAdminClientConnection;

    static TServerMainTask *s_pInstance;

    TRunningContext *m_pContext;
    int              m_nRefCount;
    TCompactTask    *m_pCompactTask;

private:
    BOOL InitNamespace();
    BOOL InitSecurity();
    void EnableBaseFile(const DBIDS &a_dbIds);
    void addJuction(TSection *a_pBase, LPCSTR a_pszSection, const SEGID_F &a_destSegID);
public:
    typedef Actor::TMessage0<TServerMainTask> Message;
public:
    inline TServerMainTask(TRunningContext *a_pContext): 
        m_mfClientConnection(this, &TServerMainTask::HandleClientConnection, false),
        m_mfAdminClientConnection(this, &TServerMainTask::HandleClientConnection, true),
        m_pContext(a_pContext),
        m_nRefCount(0), m_pCompactTask(NULL)
    {
        assert(s_pInstance == NULL);
        s_pInstance = this;
    }
    inline ~TServerMainTask()
    {
        s_pInstance = NULL;
    }

    inline static TServerMainTask* getInstance()
    {
        return s_pInstance;
    }

    virtual BOOL OnStart();
    virtual void OnBeforeRun();

    void HandleClientConnection(bool a_bAdmin);
    void ShutDown();
    void Stop();
    void AddRef();
    void Release();
};


#endif // _TREE_BASE_SVR_MAIN_