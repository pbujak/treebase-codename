// TreeBase.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "Tbcore.h"
#include <winsvc.h>
#include "Shared\TAbstractThreadLocal.h"

TThreadLocal<TTaskInfo*> G_pTask;

DWORD WINAPI LazyStartService(LPVOID lpParameter);

//*************************************************************************
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        G_hModule = hModule;
        G_hReady = CreateEvent(NULL, TRUE, FALSE, NULL);
        G_hGlobalMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, COMMON_GetGlobalMutexName());
        if(G_hGlobalMutex)
        {
            SetEvent(G_hReady);
        }
        else
        {
            DWORD  dwThreadID = 0;
            HANDLE hThread = CreateThread(NULL, 0x100000, &LazyStartService, NULL, 0, &dwThreadID);
            CloseHandle(hThread);
        }
    }
    else if (ul_reason_for_call == DLL_THREAD_DETACH)
    {
        TTaskInfo *pTask = TASK_GetCurrent();
        if(pTask != NULL)
        {
            TASK_UnregisterTask(pTask);
            delete pTask;
        }
        TAbstractThreadLocal::ClearAllData();
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        TASK_DeleteAll();
    }
    return TRUE;
}

//================================================================
class TSCHandle
{
    SC_HANDLE m_handle;
public:
    inline TSCHandle(): m_handle(NULL)
    {}
    inline TSCHandle(const SC_HANDLE &a_handle): m_handle(a_handle)
    {
    }
    ~TSCHandle()
    {
        if(m_handle) CloseServiceHandle(m_handle);
    }
    operator SC_HANDLE()
    {
        return m_handle;
    }
    operator bool()
    {
        return m_handle != NULL;
    }
};

//****************************************************************
static void RunService()
{
    TSCHandle hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT );
    if(!hSCM)
        return;
    
    TSCHandle hService = OpenService(hSCM, "TreeBaseEngine", SERVICE_START | SERVICE_QUERY_STATUS);
    if(!hService)
        return;

    if(!StartService(hService, 0, NULL))
        return;

    SERVICE_STATUS_PROCESS ssStatus = {0};
    DWORD   dwBytesNeeded = 0; 

    if (!QueryServiceStatusEx( 
        hService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // information level
        (LPBYTE) &ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded )                // size needed if buffer is too small
    )
        return; 

    while(ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        Sleep(300);
        QueryServiceStatusEx( 
            hService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE) &ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded );
    }

    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, COMMON_GetGlobalMutexName());
    InterlockedExchangePointer(&G_hGlobalMutex, hMutex);
}

//****************************************************************
DWORD WINAPI LazyStartService(LPVOID lpParameter)
{
    RunService();
    SetEvent(G_hReady);
    return 1;
}