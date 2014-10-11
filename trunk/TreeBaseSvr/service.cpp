#include "stdafx.h"
#include "service.h"
#include "util.h"
#include "TreeBaseSvr.h"
#include <winsvc.h>
#include <cstring>
#include <shlwapi.h>
#include <atlsecurity.h>

#define SVC_ERROR ((DWORD)0xC0020001L)

static LPCSTR STR_LOCAL_SERVICE = "NT AUTHORITY\\LocalService";
static LPCSTR STR_SERVICE_NAME = "TreeBaseEngine";

static SERVICE_DESCRIPTION s_description = 
{	"\"TreeBase\" database engine. "
	"When this service is disabled applications that use \"TreeBase\" may not work"
};

typedef TGenericHandle<SC_HANDLE> TSCHandle;

namespace Service{

namespace{

	class TServiceContext: public TRunningContext
	{
		SERVICE_STATUS        m_status;  
		SERVICE_STATUS_HANDLE m_hStatus;
	public:
		void reportStatus(DWORD a_dwState, DWORD a_dwWin32ExitCode, DWORD a_dwWaitHing);
	public:
		TServiceContext(SERVICE_STATUS_HANDLE a_hStatus): m_hStatus(a_hStatus)
		{
			memset(&m_status, 0, sizeof(m_status));
			m_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		}
		virtual void error(LPCSTR a_pszError);
		virtual void setState(EState a_state);
		virtual void enableStop(bool a_bEnable);
	};


	//******************************************************************************
	void TServiceContext::reportStatus(DWORD a_dwState, DWORD a_dwWin32ExitCode, DWORD a_dwWaitHing)
	{
		m_status.dwCurrentState = a_dwState;
		if(a_dwWin32ExitCode == NO_ERROR)
		{
			m_status.dwWin32ExitCode = NO_ERROR;
			m_status.dwServiceSpecificExitCode = 0;
		}
		else
		{
			m_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
			m_status.dwServiceSpecificExitCode = a_dwWin32ExitCode;
		}
		m_status.dwWaitHint = a_dwWaitHing;

		if(a_dwState == SERVICE_RUNNING)
			m_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

		m_status.dwCheckPoint++;

		SetServiceStatus(m_hStatus, &m_status);
	}

	//******************************************************************************
	void TServiceContext::error(LPCSTR a_pszError)
	{
		HANDLE hEventSource = RegisterEventSource(NULL, STR_SERVICE_NAME);
		if(!hEventSource)
			return;

		LPCTSTR pszStrings[2] = {0};
		pszStrings[0] = STR_SERVICE_NAME;
		pszStrings[1] = a_pszError;

        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    SVC_ERROR,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    pszStrings,         // array of strings
                    NULL);               // no binary data

        DeregisterEventSource(hEventSource);
	}

	//******************************************************************************
	void TServiceContext::setState(TRunningContext::EState a_state)
	{
		switch(a_state)
		{
			case TRunningContext::eRunning:
				reportStatus(SERVICE_RUNNING, NO_ERROR, 0);
				break;
			
			case TRunningContext::eShutDownReady:
				reportStatus(SERVICE_STOPPED, NO_ERROR, 0);
				break;

			case TRunningContext::eStopping:
				reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
				break;

			case TRunningContext::eStopped:
				reportStatus(SERVICE_STOPPED, NO_ERROR, 0);
				Sleep(INFINITE);  // process will be terminater in the meantime
				break;
		}
	}

	//******************************************************************************
	void TServiceContext::enableStop(bool a_bEnable)
	{
		if(a_bEnable)
		{
			m_status.dwControlsAccepted |= SERVICE_ACCEPT_STOP;
		}
		else
			m_status.dwControlsAccepted &= ~SERVICE_ACCEPT_STOP;

		SetServiceStatus(m_hStatus, &m_status);
	}


	//******************************************************************************
	inline void Alert(LPCSTR a_pszFormat, ...)
	{
		va_list vl;
		va_start(vl, a_pszFormat);

		CString strError;
		strError.FormatV(a_pszFormat, vl);

	    MessageBox(NULL, strError, STR_SERVICE_NAME, MB_ICONSTOP);
	}

	//******************************************************************************
	VOID WINAPI SvcCtrlHandler( DWORD dwCtrl )
	{
		TServerMainTask *pMainTask = TServerMainTask::getInstance();

		if(pMainTask == NULL)
			return;

		switch(dwCtrl) 
		{  
			case SERVICE_CONTROL_STOP: 
			{
				TServerMainTask::Message *pMsgStop = new TServerMainTask::Message();
				pMsgStop->Send(pMainTask, &TServerMainTask::Stop);
				break;
			}
	 
			case SERVICE_CONTROL_SHUTDOWN: 
			{
				TServerMainTask::Message *pMsgShutDown = new TServerMainTask::Message();
				pMsgShutDown->Send(pMainTask, &TServerMainTask::ShutDown);
				break;
			}
	 
			default: 
				break;
	   } 
	   
	}

	//******************************************************************************
	VOID WINAPI SvcEntry( DWORD dwArgc, LPTSTR *lpszArgv )
	{
		SERVICE_STATUS_HANDLE hStatus = RegisterServiceCtrlHandler(STR_SERVICE_NAME, &SvcCtrlHandler);

		TServiceContext ctx(hStatus);
		ctx.reportStatus(SERVICE_START_PENDING, NO_ERROR, 2000);

		TServerMainTask mainTask(&ctx);
		if(!mainTask.RunInCurrentThread(0x1000, 0x100000))
		{
			ctx.reportStatus(SERVICE_STOPPED, 1, 0);
		}
	}

}

//********************************************************************************
bool Install()
{
	TSCHandle hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCM)
	{
		Alert("Cannot access service database");
		return false;
	}

	TSCHandle hService = OpenService(hSCM, STR_SERVICE_NAME, SERVICE_QUERY_CONFIG);
	if(hService)
	{
		MessageBox(NULL, "Service already installed", STR_SERVICE_NAME, MB_ICONINFORMATION);
		return true;
	}

	char szModule[MAX_PATH] = {0};
	char szDataDir[MAX_PATH] = {0};

	GetModuleFileName(GetModuleHandle(NULL), szModule, sizeof(szModule)-1);
	strcpy(szDataDir, szModule);
	PathRemoveFileSpec(szDataDir);
	PathAppend(szDataDir, "Data");
	CreateDirectory(szDataDir, NULL);
	//-----------------------------------------------------------
	{
		ATL::CDacl dacl;

		if(!AtlGetDacl(szDataDir, SE_FILE_OBJECT, &dacl))
		{
			Alert("Cannot get security from path \"%s\"", szDataDir);
			return false;
		}
		
		CSid sidLocalService;
		VERIFY(sidLocalService.LoadAccount(STR_LOCAL_SERVICE));

		dacl.AddAllowedAce(sidLocalService, FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE);
		AtlSetDacl(szDataDir, SE_FILE_OBJECT, dacl);

		PathAppend(szDataDir, "*.*");
		CFileFind ff;
		BOOL bFound = ff.FindFile(szDataDir);
		while(bFound)
		{
			bFound = ff.FindNextFile();
			if(!ff.IsDirectory())
			{
				CString strPath = ff.GetFilePath();
				AtlGetDacl(strPath, SE_FILE_OBJECT, &dacl);
				dacl.AddAllowedAce(sidLocalService, FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE);
				AtlSetDacl(strPath, SE_FILE_OBJECT, dacl);
			}
		}		
	}
	//-----------------------------------------------------------
	strcat(szModule, " /start");

	hService = CreateService(
		hSCM,
		STR_SERVICE_NAME,
		"TreeBase Engine",
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		szModule,
		NULL, NULL, NULL,
		STR_LOCAL_SERVICE,
		"");

	if(!hService)
	{
		Alert("Failed to create service");
		return false;
	}
	//-----------------------------------------------------------
	// Enable START_SERVICE for authenticated users
	{ 
		ATL::CDacl dacl;
		AtlGetDacl(hService, SE_SERVICE, &dacl);

		int nCount = dacl.GetAceCount();
		ACCESS_MASK auMask = SERVICE_START | SERVICE_QUERY_STATUS;

		for(int i = 0; i < nCount; i++)
		{
			ATL::CSid sid;
			ACCESS_MASK mask;
			BYTE        type;
			dacl.GetAclEntry(i, &sid, &mask, &type);
			if(type == ACCESS_ALLOWED_ACE_TYPE && sid == Sids::AuthenticatedUser())
			{
				auMask |= mask;
				dacl.RemoveAce(i);
				break;
			}
		}

		VERIFY(dacl.AddAllowedAce(Sids::AuthenticatedUser(), auMask));
		AtlSetDacl(hService, SE_SERVICE, dacl);
	}
	//-----------------------------------------------------------
	ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &s_description);
	MessageBox(NULL, "Service successfully installed", STR_SERVICE_NAME, MB_ICONINFORMATION);
	return true;
}

//********************************************************************************
bool Run()
{
    static SERVICE_TABLE_ENTRY DispatchTable[] = 
    { 
        { const_cast<LPSTR>(STR_SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION) SvcEntry }, 
        { NULL, NULL } 
    }; 
	if(!StartServiceCtrlDispatcher(DispatchTable))
	{
		return false;
	};
	return true;
}

//********************************************************************************
bool Stop()
{
	TSCHandle hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if(!hSCM)
	{
		Alert("Cannot access service database");
		return false;
	}
	TSCHandle hService = OpenService(hSCM, STR_SERVICE_NAME, SERVICE_STOP);
	
	SERVICE_STATUS status = {0};
	return (hService && ControlService(hService, SERVICE_CONTROL_STOP, &status));
}

//********************************************************************************
bool UnInstall()
{
	TSCHandle hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCM)
	{
		Alert("Cannot access service database");
		return false;
	}

	TSCHandle hService = OpenService(hSCM, STR_SERVICE_NAME, DELETE);
	if(!hService)
	{
		MessageBox(NULL, "Service not installed", STR_SERVICE_NAME, MB_ICONINFORMATION);
		return true;
	}
	if(!DeleteService(hService))
	{
		Alert("Failed to delete service");
		return false;
	}
	MessageBox(NULL, "Service successfully uninstalled", STR_SERVICE_NAME, MB_ICONINFORMATION);
	return true;
}

} // namespace Service