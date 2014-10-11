// BitmapWnd.cpp : implementation file
//

#include "stdafx.h"
#include "TBTest.h"
#include "BitmapWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBitmapWnd

CBitmapWnd::CBitmapWnd()
{
}

CBitmapWnd::~CBitmapWnd()
{
}


BEGIN_MESSAGE_MAP(CBitmapWnd, CWnd)
	//{{AFX_MSG_MAP(CBitmapWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CBitmapWnd message handlers

void CBitmapWnd::DoModal(HBITMAP a_hBmp, LPCSTR a_pszName)
{
    CRect  rc;

    m_bmp.Attach(a_hBmp);

    m_bmp.GetBitmap(&m_bmpInfo);
     
    rc.left = 100;
    rc.top  = 100;
    rc.right  = 100+m_bmpInfo.bmWidth;
    rc.bottom = 100+m_bmpInfo.bmHeight;

    if (!CreateEx(0, NULL, a_pszName, WS_OVERLAPPED|WS_SYSMENU|WS_BORDER, rc, NULL, NULL))
        return;

    ShowWindow(SW_SHOW);

    RunModalLoop();
}

void CBitmapWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    memDC.SelectObject(&m_bmp);

    dc.BitBlt(0,0,m_bmpInfo.bmWidth,m_bmpInfo.bmHeight,&memDC, 0, 0, SRCCOPY);
}



BOOL CBitmapWnd::PreCreateWindow(CREATESTRUCT& cs) 
{
    cs.lpszClass = AfxRegisterWndClass(0, NULL, NULL, NULL);
	return TRUE;
}

int CBitmapWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	
	return 0;
}

void CBitmapWnd::OnClose() 
{
	EndModalLoop(0);
}
