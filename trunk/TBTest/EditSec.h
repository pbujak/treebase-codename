#if !defined(AFX_EDITSEC_H__975BA276_43E8_4CF1_A8EC_E3CDC2BD9F02__INCLUDED_)
#define AFX_EDITSEC_H__975BA276_43E8_4CF1_A8EC_E3CDC2BD9F02__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EditSec.h : header file
//
#include "treebase.h"

/////////////////////////////////////////////////////////////////////////////
// CEditSec dialog

class CEditSec : public CDialog
{
    CTBSection *m_pSection;
// Construction
public:
	CEditSec(CTBSection* a_pSection=NULL, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CEditSec)
	enum { IDD = IDD_EDITSEC };
	CString	m_Address;
	CString	m_Name;
	long	m_Age;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEditSec)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEditSec)
	afx_msg void OnKillfocus();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITSEC_H__975BA276_43E8_4CF1_A8EC_E3CDC2BD9F02__INCLUDED_)
