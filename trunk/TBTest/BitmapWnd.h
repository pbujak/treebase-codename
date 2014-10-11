#if !defined(AFX_BITMAPWND_H__5AB72C78_1A88_453B_9892_0B8A716724F7__INCLUDED_)
#define AFX_BITMAPWND_H__5AB72C78_1A88_453B_9892_0B8A716724F7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// BitmapWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBitmapWnd window

class CBitmapWnd : public CWnd
{
// Construction
public:
	CBitmapWnd();

// Attributes
public:
    CBitmap m_bmp;
    BITMAP  m_bmpInfo;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBitmapWnd)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	void DoModal(HBITMAP a_hBmp, LPCSTR a_pszName);
	virtual ~CBitmapWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CBitmapWnd)
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BITMAPWND_H__5AB72C78_1A88_453B_9892_0B8A716724F7__INCLUDED_)
