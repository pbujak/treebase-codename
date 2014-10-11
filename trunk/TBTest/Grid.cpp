// Grid.cpp : implementation file
//


#include "stdafx.h"
#include "Grid.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define FBWD_IMMEDIATE 0
#define FBWD_SEPARATOR 1
#define ZEROTEXT(x) x[0]=0
#define IS_TEXTNULL(x) x[0]==0

IMPLEMENT_DYNAMIC(CGDVException, CException)


/////////////////////////////////////////////////////////////////////////////
// CGridCtrl

CGridCtrl::CGridCtrl()
{
}

CGridCtrl::~CGridCtrl()
{
}


BEGIN_MESSAGE_MAP(CGridCtrl, CGrid<CWnd>)
	//{{AFX_MSG_MAP(CGridCtrl)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGridCtrl message handlers

BOOL CGridCtrl::PreCreateWindow(CREATESTRUCT& cs) 
{
	//return CGrid<CWnd>::PreCreateWindow(cs);
	cs.lpszClass = _T("GRIDBOX");
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CGridView

IMPLEMENT_DYNCREATE(CGridView, CCtrlView)

CGridView::CGridView() : CGrid<CCtrlView>(_T("GRIDBOX"), AFX_WS_DEFAULT_VIEW )
{

}

CGridView::~CGridView()
{
}


BEGIN_MESSAGE_MAP(CGridView, CGrid<CCtrlView>)
	//{{AFX_MSG_MAP(CGridView)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGridView diagnostics

#ifdef _DEBUG
void CGridView::AssertValid() const
{
	CGrid<CCtrlView>::AssertValid();
}

void CGridView::Dump(CDumpContext& dc) const
{
	CGrid<CCtrlView>::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGridView message handlers

char CGridDataExchange::mDecSeparator[] = {-1,-1,-1,-1,-1,-1};
char CGridDataExchange::mThSeparator[]  = {-1,-1,-1,-1,-1,-1};


void CGridDataExchange::GetText(LPSTR pTxtBuff, int pMaxLen){
	switch (mCode){
		case GBC_MODIFIED:{
			LPSTR lBuffStr = mEditInfo->bufor;
			if (pMaxLen!=-1){
				if (strlen(lBuffStr)>pMaxLen){
					THROW(new CGDVException(pMaxLen));
				}
			}
			strcpy(pTxtBuff, lBuffStr);
			break;
		}
		default:{
			ASSERT(FALSE);
		}
	}
}

void CGridDataExchange::SetText(LPSTR pTxtBuff, int pMaxLen){
	switch (mCode){
		case GBC_GET_VHDTEXT:
		case GBC_GETTEXT:{
			strcpy(mGetText[mGetTextIndex].str, pTxtBuff);
			break;
		}
		case GBC_STARTEDIT:{
			mEditInfo->type = GB_TEXTEDIT;
			mEditInfo->maxChars = pMaxLen;
			strcpy(mEditInfo->bufor, pTxtBuff);
			break;
		}
		default:{
			ASSERT(FALSE);
		}
	}
}

void CGridDataExchange::SetIndex(int pIndex){
	ASSERT(mCode==GBC_STARTEDIT);
	mEditInfo->type = GB_LBEDIT;
	mEditInfo->lbIndex = pIndex;
}

int CGridDataExchange::GetIndex(){
	ASSERT(mCode==GBC_MODIFIED);
	ASSERT(mEditInfo->type==GB_LBEDIT);
	return mEditInfo->lbIndex;
}

void CGridDataExchange::DXText(LPSTR pTextBuff, int pMaxLen){
	if (mCode==GBC_CUSTOMDRAW) return;
	if (mCode==GBC_MODIFIED){
		GetText(pTextBuff, pMaxLen);
	}else{
		SetText(pTextBuff);
	}
}

void CGridDataExchange::DXText(CString *pStr, int pMaxLen){
	if (mCode==GBC_CUSTOMDRAW) return;
	if (mCode==GBC_MODIFIED){
		char lTextBuff[255];
		GetText(lTextBuff, pMaxLen);
		*pStr = lTextBuff;
	}else{
		CString lStr = *pStr;
		SetText((LPSTR)(LPCSTR)lStr.Left(254), pMaxLen);
	}
}

void CGridDataExchange::DXIndex(int *pIndex){
	if (mCode==GBC_CUSTOMDRAW) return;
	ASSERT((mCode!=GBC_GETTEXT)&&(mCode!=GBC_GET_VHDTEXT));
	if (mCode==GBC_MODIFIED){
		*pIndex = GetIndex();
	}else{
		SetIndex(*pIndex);
	}
}

void CGridDataExchange::SetDate(COleDateTime *pTime){
	switch (mCode){
		case GBC_GET_VHDTEXT:
		case GBC_GETTEXT:{
			CString lStr = pTime->Format(LOCALE_NOUSEROVERRIDE|VAR_DATEVALUEONLY);
			strcpy(mGetText[mGetTextIndex].str, (LPCSTR)lStr);
			break;
		}
		case GBC_STARTEDIT:{
			SYSTEMTIME lSysTime;
			pTime->GetAsSystemTime(lSysTime);
			mEditInfo->type = GB_DTEDIT;
			*(SYSTEMTIME*)mEditInfo->bufor = lSysTime;
			break;
		}
		default:{
			ASSERT(FALSE);
		}
	}
}

void CGridDataExchange::GetDate(COleDateTime *pTime, COleDateTime *pTmin, COleDateTime *pTmax){
	ASSERT(mEditInfo->type==GB_DTEDIT);
	ASSERT((pTmin==NULL && pTmax==NULL)||(pTmin!=NULL && pTmax!=NULL));
	switch (mCode){
		case GBC_MODIFIED:{
			COleDateTime lTime(*(SYSTEMTIME*)mEditInfo->bufor);
			if (pTmin!=NULL){
				if ((lTime<*pTmin)||(lTime>*pTmax)){
					THROW(new CGDVException(lTime, *pTmin, *pTmax));
				}
			}
			*pTime = lTime;
			break;
		}
		default:{
			ASSERT(FALSE);
		}
	}
}

//*******************************************************************************************
void CGridDataExchange::DXDate(COleDateTime *pTime, COleDateTime *pTmin, COleDateTime *pTmax)
{
	if (mCode==GBC_CUSTOMDRAW) return;
	if (mCode==GBC_MODIFIED){
		GetDate(pTime, pTmin, pTmax);
	}else{
		SetDate(pTime);
	}
}

//*******************************************************************************************
void CGridDataExchange::DXCurrency(COleCurrency *pCy, COleCurrency *pCyMin, COleCurrency *pCyMax){
	if (mCode==GBC_CUSTOMDRAW) return;
	switch (mCode){
		//===============================================
		case GBC_GETTEXT:{
			CString lStr = pCy->Format();
//			lStr = lStr.Left(lStr.GetLength()-2);
			strcpy(mGetText[mGetTextIndex].str, (LPCSTR)lStr);
			mGetText[mGetTextIndex].halign = DT_RIGHT;
			return;
		}
		//===============================================
		case GBC_STARTEDIT:{
			CString lStr = pCy->Format();
//			lStr = lStr.Left(lStr.GetLength()-2);
			strcpy(mEditInfo->bufor, (LPCSTR)lStr);
			mEditInfo->type = GB_TEXTEDIT;
			return;
		}
		//===============================================
		case GBC_MODIFIED:{
			BOOL mOK = TRUE;
			//------------------------------
			TRY{
				pCy->ParseCurrency(mEditInfo->bufor);
			}CATCH(COleException, e){
				mOK = FALSE;
			}END_CATCH;
			//------------------------------
			if (!mOK) THROW(new CGDVException("Niew³aœciwy format"));
			return;
		}
		//===============================================
	}
}


//*******************************************************************************************
void CGridDataExchange::AddStringToList(LPSTR pStr)
{
	ASSERT(mCode==GBC_STARTEDIT);
	mEditInfo->type = GB_LBEDIT;
	AddStringToGridList(mEditInfo, pStr);
}

//*******************************************************************************************
void CGridDataExchange::SetTextAlign(int pTextAlign)
{
	if ((mCode==GBC_GETTEXT)||(mCode==GBC_GET_VHDTEXT))
		mGetText[mGetTextIndex].halign = pTextAlign;
}

//******************************************************************
void CGridDataExchange::FormatNumber(float pNumber, LPSTR pTxtBuff, int pNumMode)
{
	if ((mFormatSign<'2')||(mFormatSign>'9')){
		FormatNumberFE(pNumber, pTxtBuff);
		return;
	}
	//================================
	char lLocalTxtBuff[40];
	int  lDz;
	//================================
	//if (pNumber==-1.0f){
	//	if (!pIsEdit)
	//		wsprintf(pTxtBuff, MyStrings::mBrak);
	//	return;
	//}
	int lDeg = mFormatSign - '0';
	int lImul=10, ctr;
	for (ctr=0; ctr<lDeg; ctr++){
		lImul *= 10;
	};
	float lMul = lImul;
	//--------------------------------
	int lIntVal = pNumber;
	int lFrac = (((int)(pNumber * lMul) % lImul) + 5) / 10;
	//================================
	FormatNumber(lIntVal, lLocalTxtBuff, pNumMode);
	CString lFstr;
	lFstr.Format("%d", lFrac);
	//--------------------------------
	for (ctr=0; ctr<(2-lFstr.GetLength()); ctr++){
		lFstr.Insert(0, "0");
	}
	//--------------------------------
	wsprintf(pTxtBuff, "%s%c%s", lLocalTxtBuff, mDecSeparator[0], (LPCSTR)lFstr);
}


//******************************************************************
void CGridDataExchange::FormatNumberFE(float pNumber, LPSTR pTxtBuff){
	switch (mFormatSign){
		case 'f':{
			gcvt(pNumber, 8, pTxtBuff);
			LPSTR lDecStr = strchr(pTxtBuff, '.');
			if (lDecStr!=NULL){
				lDecStr[0] = mDecSeparator[0];
			}
			break;
		}
		case 'e':case 'E':{
			int ctr;
			float lExp;
			float lNum10;
			float lTot;
			if (pNumber==0.0f){
				lExp = 0.0f;
				lNum10 = 0.0f;
				lTot = 0.0f;
			}else{
				lExp = floor(log10(pNumber));
				lNum10 = pow(10.0f, lExp);
				lTot = pNumber / lNum10;
			}
			char lTxtBuff[10];
			memset(lTxtBuff, 0, 10);
			gcvt(lTot, 9, lTxtBuff);
			for (ctr=0; ctr<10; ctr++){
				if (lTxtBuff[ctr]==0) lTxtBuff[ctr] = '0';
			}
			lTxtBuff[6] = mFormatSign;
			lTxtBuff[7] = 0;
			strcpy(pTxtBuff, lTxtBuff);
			int lIexp = lExp;
			itoa(lIexp, lTxtBuff, 10);
			strcat(pTxtBuff, lTxtBuff);
			break;
		}
	}
}

//******************************************************************
void CGridDataExchange::FormatNumber(int pNumber, LPSTR pTxtBuff, int pNumMode)
{
	char lLocalTxtBuff[50];
	int  lDz;
	//================================
	//if (pNumber==-1){
	//	if (!pIsEdit)
	//		wsprintf(pTxtBuff, MyStrings::mBrak);
	//	return;
	//}
	//================================
	if (pNumMode==FBWD_IMMEDIATE){
		wsprintf(pTxtBuff, "%d", pNumber);
	}
	//================================
	else{
		ZEROTEXT(lLocalTxtBuff);
		ZEROTEXT(pTxtBuff);
		//---------------------------
		// Formatuj text uwzglêdniaj¹c separator tysiêcy
		do{
			lDz = pNumber / 1000;
			int lReszta = pNumber % 1000;
			//-----------
			if (lDz>0){
				wsprintf(&lLocalTxtBuff[1], "%03d", lReszta);
				lLocalTxtBuff[0] = mThSeparator[0];
			}
			else{
				wsprintf(lLocalTxtBuff, "%d", lReszta);
			}	
			strcat(lLocalTxtBuff, pTxtBuff);
			strcpy(pTxtBuff, lLocalTxtBuff);
			//-----------
			pNumber = lDz;
		}while (lDz>0);	
	}
	//================================
}

//******************************************************************
void CGridDataExchange::UpdateSeparators()
{
	GetLocaleInfo(
		LOCALE_SYSTEM_DEFAULT, 
		LOCALE_STHOUSAND,
		mThSeparator,
		6);
	GetLocaleInfo(
		LOCALE_SYSTEM_DEFAULT, 
		LOCALE_SDECIMAL,
		mDecSeparator,
		1);
}

//********************************************************
void CGridDataExchange::CoreDXInt(int *pIntNum, int pMin, int pMax, BOOL pUseNull)
{
	ASSERT(pMin<=pMax);
	if (mDecSeparator[0]==-1) UpdateSeparators();
	//================================
	switch (mCode){
		case GBC_GET_VHDTEXT:
		case GBC_GETTEXT:{
			int lIntNum = *pIntNum;
			//-------------------------------------
			if ((lIntNum==-1) && pUseNull){
				ZEROTEXT(mGetText[mGetTextIndex].str);
				return;
			}
			//-------------------------------------
			FormatNumber(lIntNum, mGetText[mGetTextIndex].str, FBWD_SEPARATOR);
			SetTextAlign(DT_RIGHT);
			break;
		}
		//======================================================
		case GBC_STARTEDIT:{
			mEditInfo->type = GB_TEXTEDIT;
			int lIntNum = *pIntNum;
			//-------------------------------------
			if ((lIntNum==-1) && pUseNull){
				ZEROTEXT(mEditInfo->bufor);
				return;
			}
			//-------------------------------------
			FormatNumber(lIntNum, mEditInfo->bufor, FBWD_IMMEDIATE);
			break;
		}
		//======================================================
		case GBC_MODIFIED:{
			//-------------------------------------
			if (IS_TEXTNULL(mEditInfo->bufor) && pUseNull){
				*pIntNum=-1;
				return;
			}
			//-------------------------------------
			int lNum = atoi(mEditInfo->bufor);
			if (pMin!=pMax){
				if ((lNum<pMin)||(lNum>pMax)){
					THROW(new CGDVException(lNum, pMin, pMax));
				}
			}
			*pIntNum = lNum;
			break;
		}
		//======================================================
	}
}

//********************************************************
void CGridDataExchange::DXFloat(float *pFloatNum, float pMin, float pMax){
	if (mDecSeparator[0]==-1) UpdateSeparators();
	//================================
	switch (mCode){
		case GBC_GET_VHDTEXT:
		case GBC_GETTEXT:{
			float lFloatNum = *pFloatNum;
			FormatNumber(lFloatNum, mGetText[mGetTextIndex].str, FBWD_SEPARATOR);
			SetTextAlign(DT_RIGHT);
			break;
		}
		case GBC_STARTEDIT:{
			mEditInfo->type = GB_TEXTEDIT;
			float lFloatNum = *pFloatNum;
			FormatNumber(lFloatNum, mEditInfo->bufor, FBWD_IMMEDIATE);
			break;
		}
		case GBC_MODIFIED:{
			LPSTR lDecStr = strchr(mEditInfo->bufor, mDecSeparator[0]);
			if ((lDecStr!=NULL)&&(mFormatSign!='e')&&(mFormatSign!='E')){
				lDecStr[0] = '.';
			}
			float lNum = atof(mEditInfo->bufor);
			if (pMin!=pMax){
				if ((lNum<pMin)||(lNum>pMax)){
					THROW(new CGDVException(lNum, pMin, pMax));
				}
			}
			*pFloatNum = lNum;
			break;
		}
	}
}

//************************************************************************
void CGridDataExchange::DXTextTab(LPSTR *pTextTab, int pCount, int *pIndex, 
							      LPSTR pNoDataMsg, LPSTR pErrMsg)
{
	switch (mCode){
		case GBC_GETTEXT:{
			int lIndex = *pIndex;
			if (lIndex==-1){
				if (pNoDataMsg!=NULL) 
					strcpy(mGetText[mGetTextIndex].str, pNoDataMsg);
				return;
			};
			if ((lIndex>=pCount)||(lIndex<0)){
				if (pErrMsg!=NULL) 
					strcpy(mGetText[mGetTextIndex].str, pErrMsg);
				return;
			};
			strcpy(mGetText[mGetTextIndex].str, pTextTab[lIndex]);
			break;
		}
		case GBC_STARTEDIT:{
			int ctr;
			int lIndex = *pIndex;
			if ((lIndex>=pCount)||(lIndex<0)) lIndex = 0;
			for (ctr=0; ctr<pCount; ctr++){
				AddStringToList(pTextTab[ctr]);
			}
			mEditInfo->lbIndex = lIndex;
			break;
		}
		case GBC_MODIFIED:{
			*pIndex = mEditInfo->lbIndex;
			break;
		}
	}
}

//************************************************************************
#ifdef USE_DXSTRLIST
void CGridDataExchange::DXStrList(IStrList *pStrList, UINT *pStrID, BOOL pLock, 
								  LPSTR pNoDataMsg, LPSTR pErrMsg){
	switch (mCode){
		case GBC_GETTEXT:{
			if ((*pStrID==-1)||(pStrList->GetCount()==0)){
				if (pNoDataMsg!=NULL)
					strcpy(mGetText[mGetTextIndex].str, pNoDataMsg);
				return;
			}
			int lIndex = pStrList->BindIDtoIndex(*pStrID);
			if (lIndex==-1){
				static LPCSTR lDefErrMsg = "B³¹d!!";
				if (pErrMsg==NULL) {pErrMsg = lDefErrMsg; };
				strcpy(mGetText[mGetTextIndex].str, pErrMsg);
			}
			pStrList->GetStringInfo(lIndex, NULL, mGetText[mGetTextIndex].str, NULL);
			break;
		}
		case GBC_STARTEDIT:{
			int lCount = pStrList->GetCount();
			if (lCount==0) return;
			int lIndex;
			if (*pStrID==-1){
				lIndex = 0;
			}else
				lIndex = pStrList->BindIDtoIndex(*pStrID);
				if (lIndex==-1) lIndex = 0;
			int ctr;
			char lTxtBuff[100];
			for (ctr=0; ctr<lCount; ctr++){
				pStrList->GetStringInfo(ctr, NULL, lTxtBuff, NULL);
				AddStringToList(lTxtBuff);
			}
			mEditInfo->lbIndex = lIndex;
			break;
		}
		case GBC_MODIFIED:{
			int lIndex = mEditInfo->lbIndex;
			UINT lStrID, lPrevStrID = *pStrID;
			pStrList->GetStringInfo(lIndex, &lStrID, NULL, NULL);
			if (pLock){
				if (lPrevStrID!=-1){
					int lPrevIndex = pStrList->BindIDtoIndex(lPrevStrID);
					pStrList->LockString(lPrevStrID, /*pLock=*/FALSE);
				}
				pStrList->LockString(lIndex, /*pLock=*/TRUE);
			}
			*pStrID = lStrID;
			break;
		}
	}

}
#endif