// TBTestDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TBTest.h"
#include "TBTestDlg.h"
#include "EditSec.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTBTestDlg dialog

CTBTestDlg::CTBTestDlg(CTBSection *a_pSection, CWnd* pParent /*=NULL*/)
	: CDialog(CTBTestDlg::IDD, pParent)
{
	//{{AFX_DATa_INIT(CTBTestDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATa_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_pSection = a_pSection;
}

CTBTestDlg::~CTBTestDlg()
{
    long        ctr=0, nCount = m_arrData.GetSize();
    CTBSection *pSection = NULL;

    for (ctr=0; ctr<nCount; ctr++)
    {   
        pSection = m_arrData.ElementAt(ctr);
        delete pSection;
        m_arrData.SetAt(ctr, NULL);
    }
}

void CTBTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATa_MAP(CTBTestDlg)
	DDX_Control(pDX, IDC_LIST1, m_List);
	//}}AFX_DATa_MAP
}

BEGIN_MESSAGE_MAP(CTBTestDlg, CDialog)
	//{{AFX_MSG_MAP(CTBTestDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_NEW, OnNew)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnClickList)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTBTestDlg message handlers

BOOL CTBTestDlg::OnInitDialog()
{
    CTBValue xCounter;

	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	
    m_List.InsertColumn(0, "Nazwa",    LVCFMT_LEFT, 90);
    m_List.InsertColumn(1, "Nazwisko", LVCFMT_LEFT, 120, 1);
    m_List.InsertColumn(2, "Adres",    LVCFMT_LEFT, 120, 2);
    m_List.InsertColumn(3, "Wiek",     LVCFMT_LEFT, 40,  3);

    try
    {
        if (m_pSection)
        {
            m_pSection->GetValue("Counter", &xCounter);
            m_nCounter = xCounter->data.int32;
        }
    }
    catch(CException *e)
    {
        e->ReportError(MB_ICONSTOP);
        return FALSE;
    }

    FillData();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTBTestDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTBTestDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

//********************************************************
void CTBTestDlg::AddRecord(LPCSTR a_pszName0, CTBSection *a_pSection)
{
    CString   strName, strAddress, strAge;
    long      nAge = 0;
    CTBValue  xValue;  
    long         nCount = m_List.GetItemCount();
    LVITEM       item = {0}; 

    a_pSection->GetValue("Nazwisko", &xValue);
    if (xValue.m_pValue)
    {
        strName = xValue->data.text;
    }
    a_pSection->GetValue("Adres", &xValue);
    if (xValue.m_pValue)
    {
        strAddress = xValue->data.text;
    }
    a_pSection->GetValue("Wiek", &xValue);
    if (xValue.m_pValue)
    {
        nAge = xValue->data.int32;
    }
    //-------------------------------------------------
    strAge.Format("%d", nAge);

    m_List.InsertItem(nCount, a_pszName0);

    m_List.SetItemText(nCount, 1, strName);
    m_List.SetItemText(nCount, 2, strAddress);
    m_List.SetItemText(nCount, 3, strAge);

    m_List.SetItemData(nCount, (DWORD)m_arrData.Add(a_pSection));
}

//********************************************************
void CTBTestDlg::FillData()
{
    CTBSection *pSection = NULL;
    BOOL        bFound      = FALSE;
    CString     strName;
    DWORD       dwType = 0;

    bFound = m_pSection->GetFirstItem(strName, dwType);

    while (bFound)
    {
        if (dwType==TBVTYPE_SECTION)
        {
            pSection = new CTBSection();
            pSection->Open(m_pSection, strName);
            AddRecord(strName, pSection);
        }
        bFound = m_pSection->GetNextItem(strName, dwType);
    }
};

//********************************************************
void CTBTestDlg::OnNew() 
{
    CTBSection  *pSection    = NULL;
    char         szName[128] = {0};
    CTBValue     xValCounter;

    wsprintf(szName, "NAME %d", m_nCounter);

    m_nCounter++;

    try
    {
        xValCounter = m_nCounter;
        m_pSection->SetValue("Counter", &xValCounter);
        m_pSection->Update();

        pSection = new CTBSection();
        pSection->Create(m_pSection, szName, NULL/*fill later*/);
        AddRecord(szName, pSection);
    }
    catch(CException *e)
    {
        e->ReportError(MB_ICONSTOP);
    }
}

//*****************************************************************
void CTBTestDlg::OnClickList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    UINT flags;
    long nItem;
    CPoint pt;
    CTBSection *pSection = NULL;
    CTBValue    xValue;

    GetCursorPos(&pt);
    m_List.ScreenToClient(&pt);
    flags = 0; 
    nItem = m_List.HitTest(pt, &flags);

    if (nItem==-1)
        return;

    pSection = m_arrData[m_List.GetItemData(nItem)];

    //--------------------------------------------------------------
    try{
        pSection->Edit();

        CEditSec editSec(pSection);
        if (editSec.DoModal()==IDOK)
        {
            pSection->Update();
        }
        else
            pSection->CancelUpdate();

        //--------------------------------------------------------------
        pSection->GetValue("Nazwisko", &xValue);
        if (xValue.m_pValue)
            m_List.SetItemText(nItem, 1, xValue->data.text);

        pSection->GetValue("Adres", &xValue);
        if (xValue.m_pValue)
            m_List.SetItemText(nItem, 2, xValue->data.text);

        pSection->GetValue("Wiek", &xValue);
        if (xValue.m_pValue)
        {
            CString strAge;
            strAge.Format("%d", xValue->data.int32);
            m_List.SetItemText(nItem, 3, strAge);
        }
    }
    catch (CException *e)
    {
        e->ReportError(MB_ICONSTOP);
    }
    //--------------------------------------------------------------
	*pResult = 0;
}

//*****************************************************************
void CTBTestDlg::OnDelete() 
{
    POSITION    pos       = m_List.GetFirstSelectedItemPosition();  
	int         nItem     = m_List.GetNextSelectedItem(pos);
    CTBSection *pSection  = nItem>=0 ? m_arrData[m_List.GetItemData(nItem)] : NULL;
    CString     strName;
    CString     strSName;
    DWORD       dwType = 0;
    BOOL        bFound = 0;

    pSection->GetName(FALSE, strSName);

    bFound = pSection->GetFirstItem(strName, dwType);
    while (bFound)
    {
        pSection->DeleteValue(strName, DELHINT_AUTO);
        bFound = pSection->GetNextItem(strName, dwType);
    }
    pSection->Update();
    delete pSection;

    m_pSection->DeleteSection(strSName);

    m_List.DeleteItem(nItem);
}
