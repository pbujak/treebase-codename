#if !defined(AFX_EXPLORER_H__04944003_0795_4502_90CD_34E452F0C591__INCLUDED_)
#define AFX_EXPLORER_H__04944003_0795_4502_90CD_34E452F0C591__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Explorer.h : header file
//
#include <afxtempl.h>
#include "afxwin.h"

class CTBSection;

struct TExpItem
{
public:
    CString  m_strName;
    DWORD    m_dwType;
    CTBValue m_value;
    long     m_nItemID; 
};

/////////////////////////////////////////////////////////////////////////////
// CExplorer dialog

class CExplorer : public CDialog
{
// Construction
public:
	void ViewImage(CTBSection *a_pSection, LPCSTR a_pszName);
	BOOL EditValue(CString &a_strName, CTBValue *a_pValue);
    TExpItem* GetActiveItem();
    TExpItem* GetFirstSelectedItem(POSITION &a_rPosition);
    TExpItem* GetNextSelectedItem(POSITION &a_rPosition);

	TExpItem* ItemFromPoint(CPoint a_pt);
	void DisplayList();
    void DisplayItem(long a_nPos, TExpItem *a_pItem);

	CTBSection* GetActiveSection();
	void LoadList();
	UINT Explorer_DoModal(CTBSection *a_pSection);
	CExplorer(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CExplorer)
	enum { IDD = IDD_EXPLORER };
	CListCtrl	m_ctlList;
	CEdit	m_ctlPath;
	CString	m_strLabel;
	//}}AFX_DATA
    CMemFile   m_file;
    CImageList m_il;
    CString    m_strLinkPath, m_strLinkName;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CExplorer)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
    CArray<CTBSection*, CTBSection*> m_secStack;
    CArray<TExpItem, TExpItem&>      m_items;   

	// Generated message map functions
	//{{AFX_MSG(CExplorer)
	virtual BOOL OnInitDialog();
	afx_msg void OnClickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBack();
	afx_msg void OnNewSection();
	afx_msg void OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDelete();
	afx_msg void OnRclickList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnKillfocusLabel();
	afx_msg void OnDeleteLabel();
	afx_msg void OnEditValue();
	afx_msg void OnAddValue();
	afx_msg void OnUpdate();
	afx_msg void OnRollback();
	afx_msg void OnAddImage();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
    CComboBox m_ctlTestCases;
public:
    afx_msg void OnRunTest();
public:
    afx_msg void OnRunAllTests();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EXPLORER_H__04944003_0795_4502_90CD_34E452F0C591__INCLUDED_)
