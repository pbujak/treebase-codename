// TBTest.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "TBTest.h"
#include "TBTestDlg.h"
#include "Explorer.h"
#include "TestFramework.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTBTestApp

BEGIN_MESSAGE_MAP(CTBTestApp, CWinApp)
	//{{AFX_MSG_MAP(CTBTestApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTBTestApp construction

CTBTestApp::CTBTestApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTBTestApp object

CTBTestApp theApp;

//********************************************************************************************
static void runRobustness()
{
    const char *szNames[] = {"Ala","Ola","Kot","Pies"};

    LPCSTR pszName = szNames[testing::getThreadIndex()];

TRACE( "%s\n", pszName);
    CTBSection   xSection, xUpperSection;

    try
    {
        xUpperSection.Open(&CTBSection::tbRootSection, "CurrentUser");

        xSection.Create(&xUpperSection, pszName, 0);
        {
            for(int ctr = 0; ctr < 1500; ctr++)
            {
                CTBValue val1;
                val1 = (long)35;
                CString strName;
                strName.Format("%s:Age %d", pszName, ctr);
                xSection.SetValue(strName, &val1);

                CTBValue val2;
                val2 = "Any name";
                strName.Format("%s:Name %d", pszName, ctr);
                xSection.SetValue(strName, &val2);
                xSection.Update();
            }
        }
    }
    catch(CTBException *e)
    {
        TRACE("**[%X] CTBException: \n" \
              "   code %d \n" \
              "   section %s\n" \
              "   item %s\n",
              GetCurrentThreadId(),
              e->m_errInfo.dwErrorCode, 
              e->m_errInfo.szErrorSection,
              e->m_errInfo.szErrorItem);

        testing::setResult(false);
    }
}

DEFINE_TEST_CASE(Robustness, 4, runRobustness)

/////////////////////////////////////////////////////////////////////////////
// CTBTestApp initialization

BOOL CTBTestApp::InitInstance()
{
    LPCSTR       pszSection = "\\Applications\\TBTest";
    BOOL         bExist     = FALSE;
    CTBSection   xSection;

    BOOL bExpl = TRUE;
    if (bExpl)
    {
        CExplorer xExplorer;
        xExplorer.Explorer_DoModal(&CTBSection::tbRootSection);
        return TRUE;
    }

	AfxEnableControlContainer();
    
    if (!TBASE_SectionExists(TBSECTION_ROOT, pszSection, &bExist))
        return FALSE;

    try
    {
        CTBValue xCounter(0l);
        if (!bExist)
        {
            xSection.Create(&CTBSection::tbRootSection, "Applications", NULL);
            m_xSection.Create(&xSection, "TBTest", NULL);
            m_xSection.SetValue("Counter", &xCounter);
            m_xSection.Update();
        }
        else
            m_xSection.Open(&CTBSection::tbRootSection, pszSection);

        //-------------------------------------------------------------------
        {
    //          TBASE_DeleteValue(m_hSection, "LongArray", DELHINT_AUTO);  
//            TBASE_VALUE *pValue = NULL;//CreateLBValue();
//            TBASE_GetValue(m_hSection, "LongArray", &pValue);
        }
        CTBValue xValue;

        xValue = CRect(0,0,10,10);
        m_xSection.SetValue("NAME 12b", &xValue);

        m_xSection.DeleteValue("NAME 12b", DELHINT_SIMPLE);
	    // Standard initialization
	    // If you are not using these features and wish to reduce the size
	    //  of your final executable, you should remove from the following
	    //  the specific initialization routines you do not need.
    }
    catch (CTBException *e)
    {
        e->ReportError(MB_OK);
        return FALSE;
    }

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CTBTestDlg dlg(&m_xSection);
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

//********************************************************************
TBASE_VALUE* CTBTestApp::CreateLBValue()
{
    TBASE_VALUE_DATA xData;
    HGLOBAL          hGlobal = NULL;
    long            *pnArray = NULL; 
    long             ctr = 0;

    memset(&xData, 0, sizeof(xData));

    hGlobal = GlobalAlloc(GMEM_MOVEABLE, 5000*sizeof(long));
    pnArray = (long*)GlobalLock(hGlobal);
    for (ctr=0; ctr<5000; ctr++)
    {
        pnArray[ctr] = ctr;
    }
    GlobalUnlock(hGlobal);
    xData.blob.cbSize = 5000*sizeof(long);
    xData.blob.hData  = hGlobal;

    return TBASE_AllocValue(TBVTYPE_LONGBINARY, &xData, -1);
}
