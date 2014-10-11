// EditSec.cpp : implementation file
//

#include "stdafx.h"
#include "TBTest.h"
#include "EditSec.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CEditSec dialog


CEditSec::CEditSec(CTBSection *a_pSection, CWnd* pParent /*=NULL*/)
	: CDialog(CEditSec::IDD, pParent)
{
	//{{AFX_DATa_INIT(CEditSec)
	m_Address = _T("");
	m_Name = _T("");
	m_Age = 0;
	//}}AFX_DATa_INIT
    m_pSection = a_pSection;
}


void CEditSec::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATa_MAP(CEditSec)
	DDX_Text(pDX, IDC_ADDRESS, m_Address);
	DDX_Text(pDX, IDC_NAME, m_Name);
	DDX_Text(pDX, IDC_AGE, m_Age);
	//}}AFX_DATa_MAP
}


BEGIN_MESSAGE_MAP(CEditSec, CDialog)
	//{{AFX_MSG_MAP(CEditSec)
	ON_EN_KILLFOCUS(IDC_ADDRESS, OnKillfocus)
	ON_EN_KILLFOCUS(IDC_AGE, OnKillfocus)
	ON_EN_KILLFOCUS(IDC_NAME, OnKillfocus)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditSec message handlers

void CEditSec::OnKillfocus() 
{
	CTBValue xValue;

    try{
	    UpdateData(TRUE);
    
        xValue = m_Address;
        m_pSection->SetValue("Adres", &xValue);

        xValue = m_Name;
        m_pSection->SetValue("Nazwisko", &xValue);

        xValue = m_Age;
        m_pSection->SetValue("Wiek", &xValue);

    }
    catch(CException *)
    {
    }
}

//*************************************************************
BOOL CEditSec::OnInitDialog() 
{
    CTBValue xValue;

	CDialog::OnInitDialog();
	
    if (m_pSection)
    {
	    try
        {
            m_pSection->GetValue("Nazwisko", &xValue);
            if (xValue.m_pValue)
                m_Name = xValue->data.text;

            m_pSection->GetValue("Adres", &xValue);
            if (xValue.m_pValue)
                m_Address = xValue->data.text;

            m_pSection->GetValue("Wiek", &xValue);
            if (xValue.m_pValue)
                m_Age = xValue->data.int32;
        }
        catch (CException *)
        {
        }
    }

    UpdateData(FALSE);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
