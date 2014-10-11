// main.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "TreeBaseSvr.h"
#include "service.h"

#define WM_STOP WM_USER + 0x0

static THandle s_hEventShutDown = CreateEvent(NULL, FALSE, FALSE, NULL);

static const char *TREE_BASE_CTLHANDLER = "TreeBaseControlHandler";

int CALLBACK TreeBaseSvrMain(HINSTANCE a_phInstance);

//==================================================================
class TAppRunningContext: public TRunningContext
{
protected:
    virtual void error(LPCSTR a_pszError);
    virtual void setState(EState a_state);
};

static TAppRunningContext S_appRunningContext;

//************************************************************************
void TAppRunningContext::error(LPCSTR a_pszError)
{
    MessageBox(NULL, a_pszError, "TreeBaseSvr", MB_ICONSTOP);
}

//************************************************************************
void TAppRunningContext::setState(TAppRunningContext::EState a_state)
{
    switch(a_state)
    {
        case TAppRunningContext::eShutDownReady:
            SetEvent(s_hEventShutDown);
            break;
        case TAppRunningContext::eStopped:
            ExitProcess(0);
            break;
    }
}

//************************************************************************
LRESULT CALLBACK ControlWindowProc(
  HWND hwnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_QUERYENDSESSION:
        {
            TServerMainTask *pMainTask = TServerMainTask::getInstance();
            TServerMainTask::Message *pMsgShutDown = new TServerMainTask::Message();
            pMsgShutDown->Send(pMainTask, &TServerMainTask::ShutDown);

            return 0;
        }
        case WM_STOP:
        {
            TServerMainTask *pMainTask = TServerMainTask::getInstance();
            TServerMainTask::Message *pMsgShutDown = new TServerMainTask::Message();
            pMsgShutDown->Send(pMainTask, &TServerMainTask::Stop);

            return 0;
        }
        case WM_ENDSESSION:
        {
            WaitForSingleObject(s_hEventShutDown, INFINITE);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


//************************************************************************
static int RunAsApplication(HINSTANCE a_hInstance)
{
    WNDCLASS wc = {0};
    wc.hInstance = a_hInstance;
    wc.lpszClassName = TREE_BASE_CTLHANDLER;
    wc.lpfnWndProc = ControlWindowProc;
    ASSERT(::RegisterClass(&wc) != NULL);

    HWND m_hMsgWindow = CreateWindowEx(WS_EX_NOACTIVATE, TREE_BASE_CTLHANDLER, TREE_BASE_CTLHANDLER, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, HWND_DESKTOP, NULL, a_hInstance, NULL);

    ASSERT(m_hMsgWindow != NULL);

    //----------------------------------------------------
    TServerMainTask *pServer = new TServerMainTask(&S_appRunningContext);
    if(!pServer->Start(0, 0x1000, 0x100000))
    {
        delete pServer;
        return -1;
    }

    //----------------------------------------------------
    MSG msg;

    while(GetMessage(&msg, NULL, 0, 0))
    {
        DispatchMessage(&msg);
    }

    return 0;
}

//************************************************************************
static int StopServer()
{
    HWND m_hMsgWindow = FindWindow(TREE_BASE_CTLHANDLER, TREE_BASE_CTLHANDLER);

    if(m_hMsgWindow != NULL)
    {
        SendMessage(m_hMsgWindow, WM_STOP, 0, 0);
        return 0;
    }

	return Service::Stop() ? 0 : -1;
}

//{{
#include "TTempFile.h"
//}}
//************************************************************************
int APIENTRY WinMain(HINSTANCE a_hInstance,
                     HINSTANCE,
                     LPSTR a_pszCmd,
                     int a_nCount)
{
    Util::TCommandLineInfo cmdLine(a_pszCmd);

    switch(cmdLine.m_eCommand)
    {
        case Util::TCommandLineInfo::eDebug:
            return RunAsApplication(a_hInstance);

		case Util::TCommandLineInfo::eInstall:
			return Service::Install() ? 0 : -1;

		case Util::TCommandLineInfo::eUnInstall:
			return Service::UnInstall() ? 0 : -1;

		case Util::TCommandLineInfo::eStart:
		{
			if(Service::Run())
				return 0;

			if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
				return RunAsApplication(a_hInstance);
		}

        case Util::TCommandLineInfo::eStop:
            return StopServer();
    }

    char buffer[256] = {0};
    DWORD length = 0;
    GetUserObjectInformation(GetProcessWindowStation(), UOI_NAME, buffer, 256, &length);
    if (stricmp(buffer, "WinSta0") == 0) 
    {
        MessageBox(NULL, "No argument", "Error", MB_ICONSTOP);
    }
    return 0;
}