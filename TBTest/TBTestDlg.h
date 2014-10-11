// TBTestDlg.h : header file
//

#if !defined(AFX_TBTESTDLG_H__2BDA80D0_3E6D_4A7B_9F91_C0369CA09C49__INCLUDED_)
#define AFX_TBTESTDLG_H__2BDA80D0_3E6D_4A7B_9F91_C0369CA09C49__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "treebase.h"
#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////
// CTBTestDlg dialog

class CTBTestDlg : public CDialog
{
// Construction
    CTBSection *m_pSection;
    long        m_nCounter; 

    CArray<CTBSection*, CTBSection*> m_arrData;
public:
	CTBTestDlg(CTBSection *a_pSection=NULL, CWnd* pParent = NULL);	// standard constructor
    ~CTBTestDlg();

// Dialog Data
	//{{AFX_DATA(CTBTestDlg)
	enum { IDD = IDD_TBTEST_DIALOG };
	CListCtrl	m_List;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTBTestDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	void FillData();
    void AddRecord(LPCSTR a_pszName0, CTBSection *a_pSection);


    // Generated message map functions
	//{{AFX_MSG(CTBTestDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnNew();
	afx_msg void OnClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDelete();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TBTESTDLG_H__2BDA80D0_3E6D_4A7B_9F91_C0369CA09C49__INCLUDED_)
