#if !defined(AFX_VALEDIT_H__B688C57F_D96E_4147_A28D_EE5F6938AFFE__INCLUDED_)
#define AFX_VALEDIT_H__B688C57F_D96E_4147_A28D_EE5F6938AFFE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ValEdit.h : header file
//
class CTBValue;

/////////////////////////////////////////////////////////////////////////////
// CValEdit dialog

class CValEdit : public CDialog
{
    CTBValue  *m_pValue;
    DWORD     m_dwType;
// Construction
public:
	void InitTypeCombo(DWORD a_nType);
	long ValEdit_DoModal(CString &a_strValue, CTBValue *a_pValue);
	CValEdit(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CValEdit)
	enum { IDD = IDD_VALEDIT };
	CComboBox	m_ctlValType;
	float	m_fFloat;
	float	m_fPointX;
	float	m_fPointY;
	float	m_fRectBottom;
	float	m_fRectLeft;
	float	m_fRectRight;
	float	m_fRectTop;
	long	m_nInteger;
	long	m_nPointX;
	long	m_nPointY;
	long	m_nRectBottom;
	long	m_nRectLeft;
	long	m_nRectRight;
	long	m_nRectTop;
	CString	m_strText;
	CString	m_strValName;
	CString	m_strValType;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CValEdit)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CValEdit)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnSelchangeValtype();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VALEDIT_H__B688C57F_D96E_4147_A28D_EE5F6938AFFE__INCLUDED_)
