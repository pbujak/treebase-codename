// TreeBaseSvrTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "TreeBaseSvrTest.h"
#include "TPageSequence.h"
#include "defs.h"
#include "MockTask.h"
#include "MockStorage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static TTaskMemAlloc s_malloc;

// The one and only application object
TTestCase *TTestCase::s_pHead = NULL;
CWinApp theApp;

using namespace std;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    COMMON_Initialize(&s_malloc, NULL);
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// TODO: code your application's behavior here.
	}

    int        idx   = 1;
    TTestCase *pTest = TTestCase::s_pHead;

    CString strCmdName;

    if(argc >= 2)
        strCmdName = argv[1];

    while(pTest)
    {
        TASK_FixStorage(NULL);
        //LPCSTR pszPos = strchr(pTest->m_pszName, '_');
        //int nLength = (pszPos ? (int)(pszPos - pTest->m_pszName) : 0);
        int nLength = strCmdName.GetLength();

        if(strCmdName.GetLength() == 0 || strncmp(strCmdName, pTest->m_pszName, nLength) == 0)
        {
            printf("\n%d. %s: \n%s\...\n", idx++, pTest->m_pszName, pTest->m_pszDesc);
            bool bRes;
            try
            {
                bRes = pTest->runTest();
            }
            catch(std::exception& )
            {
                bRes = false;
            }
            printf(bRes ? "PASSED\n" : "FAILED\n");
        }
        pTest = pTest->m_pNext;
    }

    printf("\nPress any key to continue...\n");
    std::getchar();
    return nRetCode;
}
