// ValEdit.cpp : implementation file
//

#include "stdafx.h"
#include "TBTest.h"
#include "ValEdit.h"
#include "TBaseObj.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CValEdit dialog


CValEdit::CValEdit(CWnd* pParent /*=NULL*/)
	: CDialog(CValEdit::IDD, pParent)
{
	//{{AFX_DATa_INIT(CValEdit)
	m_fFloat = 0.0f;
	m_fPointX = 0.0f;
	m_fPointY = 0.0f;
	m_fRectBottom = 0.0f;
	m_fRectLeft = 0.0f;
	m_fRectRight = 0.0f;
	m_fRectTop = 0.0f;
	m_nInteger = 0;
	m_nPointX = 0;
	m_nPointY = 0;
	m_nRectBottom = 0;
	m_nRectLeft = 0;
	m_nRectRight = 0;
	m_nRectTop = 0;
	m_strText = _T("");
	m_strValName = _T("");
	m_strValType = _T("");
	//}}AFX_DATa_INIT
}


void CValEdit::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATa_MAP(CValEdit)
	DDX_Control(pDX, IDC_VALTYPE, m_ctlValType);
	DDX_Text(pDX, IDC_FLOAT, m_fFloat);
	DDX_Text(pDX, IDC_FPOINT_X, m_fPointX);
	DDX_Text(pDX, IDC_FPOINT_Y, m_fPointY);
	DDX_Text(pDX, IDC_FRECT_BOTTOM, m_fRectBottom);
	DDX_Text(pDX, IDC_FRECT_LEFT, m_fRectLeft);
	DDX_Text(pDX, IDC_FRECT_RIGHT, m_fRectRight);
	DDX_Text(pDX, IDC_FRECT_TOP, m_fRectTop);
	DDX_Text(pDX, IDC_INTEGER, m_nInteger);
	DDX_Text(pDX, IDC_POINT_X, m_nPointX);
	DDX_Text(pDX, IDC_POINT_Y, m_nPointY);
	DDX_Text(pDX, IDC_RECT_BOTTOM, m_nRectBottom);
	DDX_Text(pDX, IDC_RECT_LEFT, m_nRectLeft);
	DDX_Text(pDX, IDC_RECT_RIGHT, m_nRectRight);
	DDX_Text(pDX, IDC_RECT_TOP, m_nRectTop);
	DDX_Text(pDX, IDC_TEXT, m_strText);
	DDX_Text(pDX, IDC_VALNAME, m_strValName);
	DDX_CBString(pDX, IDC_VALTYPE, m_strValType);
	//}}AFX_DATa_MAP
}


BEGIN_MESSAGE_MAP(CValEdit, CDialog)
	//{{AFX_MSG_MAP(CValEdit)
	ON_CBN_SELCHANGE(IDC_VALTYPE, OnSelchangeValtype)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CValEdit message handlers

long CValEdit::ValEdit_DoModal(CString &a_strValue, CTBValue *a_pValue)
{
    long nRetVal = IDCANCEL;
    if (a_pValue)
    {
        m_pValue = a_pValue;
        m_strValName = a_strValue;
        nRetVal = DoModal();
        if (nRetVal==IDOK)
        {
            a_strValue = m_strValName;
        }
    }
    return nRetVal;
}

BOOL CValEdit::OnInitDialog() 
{
    long nValType = 0;
	CDialog::OnInitDialog();
	
    if (m_pValue->m_pValue)
    {
        nValType = m_pValue->m_pValue->dwType;
        switch (nValType)
        {
            case TBVTYPE_INTEGER:
                m_nInteger = m_pValue->m_pValue->data.int32;
                break;
            case TBVTYPE_FLOAT  :
                m_fFloat   = m_pValue->m_pValue->data.float64;
                break;
            case TBVTYPE_TEXT   :   
                m_strText  = m_pValue->m_pValue->data.text;
                break;
            case TBVTYPE_RECT   :
                m_nRectTop    = m_pValue->m_pValue->data.rect.top;
                m_nRectBottom = m_pValue->m_pValue->data.rect.bottom;
                m_nRectLeft   = m_pValue->m_pValue->data.rect.left;
                m_nRectRight  = m_pValue->m_pValue->data.rect.right;
                break;
            case TBVTYPE_POINT  :
                m_nPointX = m_pValue->m_pValue->data.point.x;
                m_nPointY = m_pValue->m_pValue->data.point.y;
                break;
            case TBVTYPE_FRECT  :
                m_fRectTop    = m_pValue->m_pValue->data.frect.fY;
                m_fRectBottom = m_pValue->m_pValue->data.frect.fcY;
                m_fRectLeft   = m_pValue->m_pValue->data.frect.fX;
                m_fRectRight  = m_pValue->m_pValue->data.frect.fcX;
                break;
            case TBVTYPE_FPOINT :
                m_fPointX = m_pValue->m_pValue->data.fpoint.fX;
                m_fPointY = m_pValue->m_pValue->data.fpoint.fY;
                break;
        }

    }
    UpdateData(FALSE);      
    if (nValType==0)
    {
        InitTypeCombo(nValType);
    }
    else
    {
        m_dwType = nValType;
        m_ctlValType.EnableWindow(FALSE);
    }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


//**************************************************************************
void CValEdit::InitTypeCombo(DWORD a_dwType)
{
    long nIndex = -1;
    long nCount = 0;
    DWORD dwType = 0;

    nIndex = m_ctlValType.AddString("Integer");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_INTEGER);

    nIndex = m_ctlValType.AddString("Float");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_FLOAT);

    nIndex = m_ctlValType.AddString("Text");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_TEXT);

    nIndex = m_ctlValType.AddString("Rectangle");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_RECT);

    nIndex = m_ctlValType.AddString("Point");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_POINT);

    nIndex = m_ctlValType.AddString("Float Rectangle");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_FRECT);

    nIndex = m_ctlValType.AddString("Float Point");
    m_ctlValType.SetItemData(nIndex, TBVTYPE_FPOINT);

    nCount = m_ctlValType.GetCount();
    for (nIndex=0; nIndex<nCount; nIndex++)
    {
        dwType = m_ctlValType.GetItemData(nIndex);
        if (dwType==a_dwType)
        {
            m_ctlValType.SetCurSel(nIndex);
            return;
        }
    }
    m_dwType = dwType;
}

//**************************************************************************
void CValEdit::OnOK() 
{
    UpdateData(TRUE);
    switch (m_dwType)
    {
        case TBVTYPE_INTEGER:
            *m_pValue = m_nInteger;
            break;
        case TBVTYPE_FLOAT  :
            *m_pValue = m_fFloat;
            break;
        case TBVTYPE_TEXT   :   
            *m_pValue = m_strText;
            break;
        case TBVTYPE_RECT   :
        {
            CRect rc;
            rc.left = m_nRectLeft;
            rc.top  = m_nRectTop;
            rc.right = m_nRectRight;
            rc.bottom = m_nRectBottom;
            *m_pValue = rc;
            break;
        }
        case TBVTYPE_POINT  :
        {
            CPoint pt(m_nPointX, m_nPointY);
            *m_pValue = pt;
            break;
        }
        case TBVTYPE_FRECT  :
        {
            FLOAT_RECT frc = {0};
            frc.fX = m_fRectLeft;
            frc.fY = m_fRectTop;
            frc.fcX = m_fRectRight;
            frc.fcY = m_fRectBottom;
            *m_pValue = frc;
            break;
        }
        case TBVTYPE_FPOINT :
        {
            FLOAT_POINT fpt = {0};
            fpt.fX = m_nPointX;
            fpt.fY = m_nPointY;
            *m_pValue = fpt;
            break;
        }
    }
	
	CDialog::OnOK();
}

//**************************************************************************
void CValEdit::OnSelchangeValtype() 
{
	long nIndex = m_ctlValType.GetCurSel();

    m_dwType = m_ctlValType.GetItemData(nIndex);
}
