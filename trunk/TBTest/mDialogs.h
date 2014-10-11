#define PASSWD_FAIL   -2
#define PASSWD_CANCEL -1

#pragma once
#pragma pack(push)
#pragma pack(1)

#define MSD_CHANGED 0x20

//--------------------------------------------------
//   wParam - (HWND) - okno RichEdit
#define FBM_CONNECT          WM_USER+0x1
//--------------------------------------------------
#define FBM_UPDATE           WM_USER+0x2
//--------------------------------------------------
//   Zwracana wartoœæ - wartoœæ maski (CFE_BOLD|CFE_ITALIC|... etc.
#define FBM_GETACTIONMASK    WM_USER+0x3
//--------------------------------------------------
//   wParam - wartoœæ maski (CFE_BOLD|CFE_ITALIC|... etc.
#define FBM_SETACTIONMASK    WM_USER+0x4
//--------------------------------------------------
//   wParam - (MSG*) wartoœæ komunikatu
#define FBM_PREPROCESSMSG    WM_USER+0x5
//--------------------------------------------------
// kody komunikatu WM_NOTIFY przesy³ane do rozszerzenia okna dialogowego
#define MBN_BASE          0x0 - 0x700 
#define MBN_MSGBOXREADY   MBN_BASE + 0x0
#define MBN_ACTION        MBN_BASE + 0x1
#define MBN_BTN_ACTIVATE  MBN_BASE + 0x2

typedef struct{ 
	HMODULE mhResHandle;
	LPCSTR  pDlgID;
	DLGPROC mDlgProc;
	LPARAM  mInitParam;
}MSGBOXHOOK;

typedef struct{
	NMHDR hdr;
	LPSTR txt;
}MSDCHANGED;

#pragma pack(pop)


BOOL __stdcall ExeModifyStrDialog(
				 LPSTR pStrBuf, 
				 LPSTR pStrName = "Nazwa pozycji:",
				 int pMaxLen = 40,
				 BOOL pAllowEmpty = FALSE);

BOOL __stdcall ExeModifyStrDialog2(
		LPSTR pStrBuf, 
		LPSTR pStrName = "Nazwa pozycji:", 
		HWND phwndNotifyTarget = NULL,
		UINT pIDNotifyCtl = 0,
		int pMaxLen = 40, 
		BOOL pAllowEmpty = FALSE);

//**************************************************************
BOOL __stdcall ExeCheckListDialog(LPSTR *pStrTab, int pStrCnt, DWORD *pMask, LPSTR pTitle=NULL);

//**************************************************************
BOOL __stdcall ExeChooseListDialog(
		 int pListItemCnt, 
		 LPSTR *pListItems, 
		 int *pCurListItem, 
		 LPSTR pStr,
		 int   pMaxChars = 40,
		 LPSTR pTitle = "Wybierz z listy");

//**************************************************************
int __stdcall CheckPasswordEx(LPSTR pOrigPasswd[], 
				   int pOrigPwdCount,
				   int pMaxChars=10, 
			       LPSTR pCaption=NULL);

//**************************************************************
int __stdcall CheckPassword(LPSTR pOrigPasswd, 
				   int pMaxChars=10, 
			       LPSTR pCaption=NULL);


//**************************************************************
BOOL __stdcall SetNewPassword(LPSTR pOldPasswd, 
  			     LPSTR pNewPasswd, 
				 int pMaxChars=10, 
				 LPSTR pCaption=NULL);

//**************************************************************
BOOL __stdcall EnterNumber(int *pNumber, 
						   int pBaseRow, 
						   int pMin, int pMax, 
						   LPSTR pCaption);

//**************************************************************
BOOL  __stdcall EditMemo(LPSTR *pText, LPSTR pCaption);
LPSTR __stdcall CreateMemo(LPSTR pText);
LPSTR __stdcall AllocMemo(int pSize);
void  __stdcall FreeMemo(LPSTR pText);

//**************************************************************
BOOL __stdcall EditNumTab(int pCount, 
						  LPSTR *pNameTab, 
						  int *pValTab, 
						  int pBaseRow, int pMin, int pMax, 
						  LPSTR pCaption);

//***********************************************
BOOL __stdcall TrackEditDlg(LPSTR pStrBuf, 
						    int pMaxLen, 
					 	    BOOL pAllowEmpty);
//***********************************************
HWND __stdcall GetFontBar(LPSTR pName, 
						  HWND pParent);

//***********************************************
void __stdcall DestroyFontBar();

//***********************************************
void __stdcall DoRichEditMenu(HWND phwndRichEdit, LPSTR pFontBarName);

//***********************************************
int WINAPI MessageBoxAdv(
  HWND hWnd,              // handle to owner window
  LPCTSTR lpText,         // text in message box
  LPCTSTR lpCaption,      // message box title
  UINT uType,             // message box style
  MSGBOXHOOK* pHookInfo); // hook
