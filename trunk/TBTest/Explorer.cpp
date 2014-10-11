// Explorer.cpp : implementation file
//

#include "stdafx.h"
#include "TBTest.h"
#include "Explorer.h"
#include "TBAseObj.h"
#include "ValEdit.h"
#include "mdialogs.h"
#include "BitmapWnd.h"
#include "TestFramework.h"
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static int CALLBACK SORT_CompareItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    TExpItem *pItem1 = (TExpItem *)lParam1;
    TExpItem *pItem2 = (TExpItem *)lParam2;

    if (pItem1 && pItem2)
    {
        if (pItem1->m_dwType!=TBVTYPE_SECTION && pItem2->m_dwType==TBVTYPE_SECTION)
        {
            return 1;
        }
        else if (pItem1->m_dwType==TBVTYPE_SECTION && pItem2->m_dwType!=TBVTYPE_SECTION)
        {
            return -1;
        }
        else
        {
            return pItem1->m_strName.CompareNoCase(pItem2->m_strName);
        }
    }
    return 0;
};


/////////////////////////////////////////////////////////////////////////////
// CExplorer dialog


CExplorer::CExplorer(CWnd* pParent /*=NULL*/)
	: CDialog(CExplorer::IDD, pParent)
{
	//{{AFX_DATa_INIT(CExplorer)
	m_strLabel = _T("");
	//}}AFX_DATa_INIT
}


void CExplorer::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATa_MAP(CExplorer)
    DDX_Control(pDX, IDC_LIST, m_ctlList);
    DDX_Control(pDX, IDC_EDIT, m_ctlPath);
    DDX_Text(pDX, IDC_LABEL, m_strLabel);
    //}}AFX_DATa_MAP
    DDX_Control(pDX, IDC_TESTCASES, m_ctlTestCases);
}


BEGIN_MESSAGE_MAP(CExplorer, CDialog)
	//{{AFX_MSG_MAP(CExplorer)
	ON_NOTIFY(NM_CLICK, IDC_LIST, OnClickList)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST, OnDblclkList)
	ON_BN_CLICKED(IDC_BACK, OnBack)
	ON_BN_CLICKED(IDC_NEW_SECTION, OnNewSection)
	ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_LIST, OnBeginlabeleditList)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST, OnEndlabeleditList)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_NOTIFY(NM_RCLICK, IDC_LIST, OnRclickList)
	ON_COMMAND(ID_EDIT_COPY, OnCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnPaste)
	ON_EN_KILLFOCUS(IDC_LABEL, OnKillfocusLabel)
	ON_BN_CLICKED(IDC_DELETE_LABEL, OnDeleteLabel)
	ON_COMMAND(ID_EDIT_VALUE, OnEditValue)
	ON_COMMAND(ID_ADD_VALUE, OnAddValue)
	ON_BN_CLICKED(IDC_UPDATE, OnUpdate)
	ON_BN_CLICKED(IDC_ROLLBACK, OnRollback)
	ON_COMMAND(ID_DELETE, OnDelete)
	ON_COMMAND(ID_NEWSECTION, OnNewSection)
	ON_COMMAND(ID_ADD_IMAGE, OnAddImage)
	//}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_RUNTEST, &CExplorer::OnRunTest)
    ON_BN_CLICKED(IDC_RUNALLTESTS, &CExplorer::OnRunAllTests)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CExplorer message handlers

UINT CExplorer::Explorer_DoModal(CTBSection *a_pSection)
{
    m_secStack.Add(a_pSection);
    return DoModal();
}

//*********************************************************************
BOOL CExplorer::OnInitDialog() 
{
    HICON hIcon = NULL;

	CDialog::OnInitDialog();
	
	m_ctlList.InsertColumn(0, "Nazwa", LVCFMT_LEFT, 160);
	m_ctlList.InsertColumn(1, "Typ",   LVCFMT_LEFT, 110);
	m_ctlList.InsertColumn(2, "Wartoœæ",   LVCFMT_LEFT, 160);
	
    if (m_il.Create(16,16, ILC_COLOR|ILC_MASK, 1, 0))
    {
        hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FOLDER), IMAGE_ICON, 16, 16, 0);
        long s= m_il.Add(hIcon);
    };
    m_ctlList.SetImageList(&m_il, LVSIL_SMALL);
    LoadList();

    std::vector<TEST_CASE*> vecTestCases;
    TTestCaseInit::getTestCases(vecTestCases);

    for(std::vector<TEST_CASE*>::const_iterator it = vecTestCases.begin(); it != vecTestCases.end(); ++it)
    {
        TEST_CASE* pTestCase = *it;
        int id = m_ctlTestCases.AddString(pTestCase->szName);
        m_ctlTestCases.SetItemDataPtr(id, pTestCase);
    }

    m_ctlTestCases.SetCurSel(0);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

//*********************************************************************
void CExplorer::LoadList()
{
    CTBSection *pSection = GetActiveSection();
    TExpItem    xItem;
    BOOL        bFound = FALSE;
    CString     strPath;

    m_ctlList.DeleteAllItems();
    m_items.RemoveAll();

    if (pSection)
    {
        try{
            bFound = pSection->GetFirstItem(xItem.m_strName, xItem.m_dwType);
            while (bFound)
            {
                if (xItem.m_dwType!=TBVTYPE_SECTION && xItem.m_dwType!=TBVTYPE_LONGBINARY)
                {
                    pSection->GetValue(xItem.m_strName, &xItem.m_value);
                }
                m_items.Add(xItem);
                xItem.m_value.Detach();
                bFound = pSection->GetNextItem(xItem.m_strName, xItem.m_dwType);
            }
            DisplayList();
            pSection->GetName(TRUE, strPath);
            m_ctlPath.SetWindowText(strPath);
            pSection->GetLabel(m_strLabel);
        }
        catch(CTBException *e)
        {
            e->ReportError(MB_OK);
            m_items.RemoveAll();
            return;
        }
        UpdateData(FALSE);
    }
}

//*********************************************************************
CTBSection* CExplorer::GetActiveSection()
{
    CTBSection *pSection = NULL;
    long        nCount   = 0;

    nCount = m_secStack.GetSize();
    if (nCount>0)
        return m_secStack[nCount-1];
    return NULL;
}

//*********************************************************************
void CExplorer::DisplayItem(long a_nPos, TExpItem *a_pItem)
{
    long      nItem  = 0;
    long      nImage = 0;
    CString   strType, strValue;

    nImage = (a_pItem->m_dwType==TBVTYPE_SECTION ? 0 : 1);
    nItem = m_ctlList.InsertItem(a_nPos, a_pItem->m_strName, nImage);
    a_pItem->m_nItemID = nItem;

    switch (a_pItem->m_dwType)
    {
        case TBVTYPE_SECTION:
            strType = "<SUB-SECTION>";
            break;
        case TBVTYPE_INTEGER:
            strType = "Integer";
            if(a_pItem->m_value)
            {
                strValue.Format("%d", a_pItem->m_value->data.int32);
            }
            else
                strValue = "?";
            break;
        case TBVTYPE_TEXT:
            strType = "Text";
            if(a_pItem->m_value)
            {
                strValue = a_pItem->m_value->data.text;
            }
            else
                strValue = "?";
            break;
        case TBVTYPE_FLOAT:
            strType = "Real";
            if(a_pItem->m_value)
            {
                strValue.Format("%f", a_pItem->m_value->data.float64);
            }
            else
                strValue = "?";
            break;
        case TBVTYPE_POINT:
        {
            strType = "Point";
            if(a_pItem->m_value)
            {
                strValue.Format("x=%d, y=%d", a_pItem->m_value->data.point.x, a_pItem->m_value->data.point.y);
            }
            else
                strValue = "?";
            break;
        }
        case TBVTYPE_RECT:
        {
            strType = "Rectangle";
            if(a_pItem->m_value)
            {
                CRect rc(a_pItem->m_value->data.rect);
                strValue.Format("x=%d, y=%d, cx=%d, cy=%d", rc.left, rc.top, rc.Width(), rc.Height());
            }
            else
                strValue = "?";
            break;
        }
        case TBVTYPE_DATE:
        {
            if(a_pItem->m_value)
            {
                COleDateTime oleDateTime(a_pItem->m_value->data.date);
                strType = "Date";
                strValue = oleDateTime.Format(LOCALE_NOUSEROVERRIDE | VAR_DATEVALUEONLY);
            }
            else
                strValue = "?";
            break;
        }
        case TBVTYPE_LONGBINARY:
            strType = "Long Binary";
            break;
        case TBVTYPE_PTGROUP:
            strType = "Point Group";
            break;
        case TBVTYPE_FPTGROUP:
            strType = "Float Point Group";
            break;
    }
    m_ctlList.SetItemText(nItem, 1, strType);
    m_ctlList.SetItemText(nItem, 2, strValue);
    m_ctlList.SetItemData(nItem, (DWORD)a_pItem);
}

//*********************************************************************
void CExplorer::DisplayList()
{
    long      ctr=0, nCount = m_items.GetSize();
    TExpItem *pItem  = NULL;

    for (ctr=0; ctr<nCount; ctr++)
    {
        pItem = &m_items.ElementAt(ctr);
        DisplayItem(ctr, pItem);
    }
    m_ctlList.SortItems(SORT_CompareItems, 0);
}

//*****************************************************************
TExpItem* CExplorer::GetActiveItem()
{
    long      nItem = m_ctlList.GetSelectionMark();
    TExpItem *pItem = NULL;
    
    if (nItem!=-1)
    {
        return (TExpItem*)m_ctlList.GetItemData(nItem);
    }
    return NULL;
}

//*****************************************************************
TExpItem* CExplorer::GetFirstSelectedItem(POSITION &a_rPosition)
{
    a_rPosition = m_ctlList.GetFirstSelectedItemPosition();
    return GetNextSelectedItem(a_rPosition);
}

//*****************************************************************
TExpItem* CExplorer::GetNextSelectedItem(POSITION &a_rPosition)
{
    long      nItem = m_ctlList.GetNextSelectedItem(a_rPosition);
    TExpItem *pItem = NULL;
    
    if (nItem!=-1)
    {
        return (TExpItem*)m_ctlList.GetItemData(nItem);
    }
    return NULL;
}

//*****************************************************************
TExpItem* CExplorer::ItemFromPoint(CPoint a_pt)
{
    UINT nFlags = 0;
    int  nItem  = -1;

    nItem = m_ctlList.HitTest(a_pt, &nFlags);
    if (nFlags==LVHT_ONITEMLABEL)
    {
        return (TExpItem*)m_ctlList.GetItemData(nItem);
    }
    return NULL;
}

//*****************************************************************
void CExplorer::OnClickList(NMHDR* pNMHDR, LRESULT* pResult) 
{
}

//*****************************************************************
void CExplorer::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    CPoint      pt;
    TExpItem   *pItem = NULL;
    CTBSection *pSection = NULL;
    CTBSection *pNewSection = NULL;
    
    GetCursorPos(&pt);
    m_ctlList.ScreenToClient(&pt);

    pItem = ItemFromPoint(pt);
    pSection = GetActiveSection();
    if (pItem && pSection && pItem->m_dwType==TBVTYPE_SECTION)
    {
        pNewSection = new CTBSection();
        try
        {
            pNewSection->Open(pSection, pItem->m_strName);
            m_secStack.Add(pNewSection);
            LoadList();
        }
        catch(CTBException* e)
        {
            if (pNewSection!=NULL)
                delete pNewSection;
            e->ReportError(MB_OK|MB_ICONSTOP);
        }
    }

	*pResult = 0;
}

//*****************************************************************
void CExplorer::OnBack() 
{
    CTBSection *pSection = NULL;
    long        nCount   = 0;    
	
    nCount = m_secStack.GetSize();
    if (nCount>1)
    {
        pSection = m_secStack[nCount-1];
        delete pSection;
        m_secStack.RemoveAt(nCount-1);

        LoadList();
    }
}

//*****************************************************************
void CExplorer::OnNewSection() 
{
    CTBSection *pSection = GetActiveSection();
	CTBSection *pNewSection = NULL;
    TExpItem    xItem, *pItem = NULL;
    long        nIndex = -1;
    char        szBuffer[MAX_PATH] = {0};
    long        ctr = 0;

    pNewSection = new CTBSection();
    if (pSection && pNewSection)
    {
        try
        {
            do
            {
                wsprintf(szBuffer, "New Section (%d)", ctr++);
            }while (pSection->SectionExists(szBuffer));

            pNewSection->Create(pSection, szBuffer, 0);
            delete pNewSection;
            pNewSection = NULL;

        }
        catch (CTBException *e)
        {
            if (pNewSection!=NULL)
                delete pNewSection;
            e->ReportError(MB_OK|MB_ICONSTOP);
            LoadList();
            return;
        }

        xItem.m_strName = szBuffer;           
        xItem.m_dwType  = TBVTYPE_SECTION;
        nIndex = m_items.Add(xItem);
        pItem = &m_items.ElementAt(nIndex);
        DisplayItem(nIndex, pItem);
        m_ctlList.SetFocus();
        m_ctlList.EditLabel(pItem->m_nItemID);
    }
}

//*****************************************************************
void CExplorer::OnBeginlabeleditList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO *pDispInfo = (LV_DISPINFO*)pNMHDR;
    TExpItem    *pItem     = (TExpItem*)m_ctlList.GetItemData(pDispInfo->item.iItem);
	
    if (pItem)
    {
        if (pItem->m_dwType==TBVTYPE_SECTION)
        {
            CTBSection *pSection  = GetActiveSection();
            *pResult = (pSection->GetAccess() & TBACCESS_MODIFY) ? 0 : 1;
            return;
        }
    }
	*pResult = 1;
}

//*****************************************************************
void CExplorer::OnEndlabeleditList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO *pDispInfo  = (LV_DISPINFO*)pNMHDR;
    TExpItem    *pItem      = (TExpItem*)m_ctlList.GetItemData(pDispInfo->item.iItem);
    LPCSTR       pszNewName = NULL;
    CTBSection  *pSection   = GetActiveSection();
	
    if (pItem)
    {
        if (pItem->m_dwType==TBVTYPE_SECTION)
        {
            pszNewName = pDispInfo->item.pszText;
            if (pszNewName)
            {
                try
                {
                    pSection->RenameSection(pItem->m_strName, pszNewName);
                    LoadList();
                }
                catch(CTBException *e)
                {
            	    e->ReportError(MB_OK|MB_ICONSTOP);
                }
            }
        }

    }	
	*pResult = 0;
}

//*****************************************************************
void CExplorer::OnDelete() 
{
    POSITION    position = NULL;
	TExpItem   *pItem    = GetFirstSelectedItem(position);
    CTBSection *pSection = GetActiveSection();
	
    if (pSection)
    {
        try
        {
            while(pItem)
            {
                if (pItem->m_dwType == TBVTYPE_SECTION)
                {
                    pSection->DeleteSection(pItem->m_strName);
                }
                else
                {
                    pSection->DeleteValue(pItem->m_strName, DELHINT_AUTO);
                }
                pItem = GetNextSelectedItem(position);
            }
        }
        catch(CTBException *e)
        {
            e->ReportError(MB_OK|MB_ICONSTOP);
        }
        LoadList();
    }
}

//*****************************************************************
void CExplorer::OnRclickList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	CMenu  xMenu;
    CMenu *pPopupMenu = NULL;
    CPoint pt;

    GetCursorPos(&pt);

    if (xMenu.LoadMenu(IDR_LISTMENU))
    {
        pPopupMenu = xMenu.GetSubMenu(0);
        if (pPopupMenu)
        {
            pPopupMenu->TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, this);
        }
    };
 
	*pResult = 0;
}

//*****************************************************************
void CExplorer::OnCopy() 
{
	TExpItem   *pItem    = GetActiveItem();
    CTBSection *pSection = GetActiveSection();
    CString     strPath;
	
{
    pSection->Export(&m_file);
    return;
}

    if (pItem && pSection)
    {
        if (pItem->m_dwType==TBVTYPE_SECTION)
        {
            try
            {
                pSection->GetName(TRUE, strPath);
                strPath += pItem->m_strName;
                m_strLinkPath = strPath;
                m_strLinkName = pItem->m_strName;
            }
            catch(CTBException *e)
            {
                e->ReportError(MB_OK|MB_ICONSTOP);
            }
        }
    }
	
}

//*****************************************************************
void CExplorer::OnPaste() 
{
    CTBSection *pSection = GetActiveSection();
    CString     strPath;
    TExpItem    xItem, *pItem = NULL;
    long        nIndex = 0;
	
{
    pSection->Import(&m_file);
    return;
}
    if (!m_strLinkPath.IsEmpty() && pSection)
    {
        try
        {
            pSection->CreateSectionLink(&CTBSection::tbRootSection, m_strLinkPath, m_strLinkName);
        }
        catch(CTBException *e)
        {
            e->ReportError(MB_OK|MB_ICONSTOP);
            return;
        }
    }
    xItem.m_strName = m_strLinkName;           
    xItem.m_dwType  = TBVTYPE_SECTION;
    nIndex = m_items.Add(xItem);
    pItem = &m_items.ElementAt(nIndex);
    DisplayItem(nIndex, pItem);
    m_ctlList.SetFocus();
    m_ctlList.SetSelectionMark(pItem->m_nItemID);
}

//*****************************************************************
void CExplorer::OnKillfocusLabel() 
{
    CTBSection *pSection = GetActiveSection();
    CString     strLabel = m_strLabel;

    if (pSection)
    {
        UpdateData(TRUE);
        if (strLabel.Compare(m_strLabel)!=0)
        {
            try
            {
                pSection->SetLabel(m_strLabel);    
            }
            catch(CTBException *e)
            {
                e->ReportError(MB_OK|MB_ICONSTOP);
                m_strLabel = strLabel;
                UpdateData(FALSE);
            }
        }
    }	
}

//*****************************************************************
void CExplorer::OnDeleteLabel() 
{
    CTBSection *pSection = GetActiveSection();
    CString     strLabel = m_strLabel;

    if (pSection)
    {
        UpdateData(TRUE);
        try
        {
            pSection->DeleteLabel(); 
            m_strLabel.Empty();
            UpdateData(FALSE);
        }
        catch(CTBException *e)
        {
            e->ReportError(MB_OK|MB_ICONSTOP);
            m_strLabel = strLabel;
            UpdateData(FALSE);
        }
    }	
}

//*****************************************************************
void CExplorer::OnEditValue() 
{
    CTBSection *pSection = GetActiveSection();
	TExpItem   *pItem = GetActiveItem();
    CTBValue    xValue;
	
    if (!pSection)
        return;
    if (pItem)
    {
        if(pItem->m_dwType==TBVTYPE_LONGBINARY)
        {
            try
            {
                ViewImage(pSection, pItem->m_strName);
            }
            catch (CException *e)
            {
                e->ReportError(MB_OK|MB_ICONSTOP);
            }
        }
        else if(pItem->m_dwType!=TBVTYPE_SECTION)
        {
            EditValue(pItem->m_strName, &pItem->m_value);
        }
    }
}

//*****************************************************************
void CExplorer::OnAddValue() 
{
	CTBValue    xValue;
    CTBSection *pSection = GetActiveSection();
    CString     strValue("New value");
    try
    {
        pSection->Edit();
	    EditValue(strValue, &xValue);
    }
    catch (CTBException *e)
    {
        e->ReportError(MB_OK|MB_ICONSTOP);
    }
}

//*****************************************************************
BOOL CExplorer::EditValue(CString &a_strName, CTBValue *a_pValue)
{
    CValEdit    xValEdit;
    CTBSection *pSection = GetActiveSection();
    
    if (pSection)
    {
        if(xValEdit.ValEdit_DoModal(a_strName, a_pValue)==IDOK)
        {
            try
            {
                pSection->SetValue(a_strName, a_pValue);                
                LoadList();
                return TRUE;
            }
            catch (CTBException *e)
            {
                e->ReportError(MB_OK|MB_ICONSTOP);
            }
        };
    }
    return FALSE;
}

//*****************************************************************
void CExplorer::OnUpdate() 
{
    CTBSection *pSection = GetActiveSection();
    if (pSection)
    {
        try
        {
            pSection->Update();                
        }
        catch (CTBException *e)
        {
            e->ReportError(MB_OK|MB_ICONSTOP);
        }
    }
    LoadList();
}

//*****************************************************************
void CExplorer::OnRollback() 
{
    CTBSection *pSection = GetActiveSection();
    if (pSection)
    {
        try
        {
            pSection->CancelUpdate();                
        }
        catch (CTBException *e)
        {
            e->ReportError(MB_OK|MB_ICONSTOP);
        }
    }
    LoadList();
}

//*****************************************************************
void CExplorer::OnAddImage() 
{
    CFile       file;  
    CFileDialog dlg(TRUE, NULL, NULL, 0, "Bitmap files|*.bmp");
    long        nSize = 0, nOfs = 0, nRead = 0;
    LPBYTE      byData = NULL;
    HGLOBAL     hData  = NULL;
    CTBValue    xValue;  
    char        szName[256] = {0};
    CTBSection *pSection = GetActiveSection();

    if (!pSection)
        return;
    if (!ExeModifyStrDialog(szName, "Value name"))
        return;

    if(dlg.DoModal()!=IDOK)
        return;

    file.Open(dlg.GetFileName(), CFile::modeRead);
    pSection->SetLongBinary(szName, &file);
    file.Close();

    LoadList();
}

//*****************************************************************
void CExplorer::ViewImage(CTBSection *a_pSection, LPCSTR a_pszName)
{
    HGLOBAL hData  = NULL;
    LPVOID  pvData = NULL;
    long    nSize  = -1;     
    CFile   xFile;

    const char *pszFile = _tempnam(NULL, "TB_Img");
    {
        xFile.Open(pszFile, CFile::modeWrite|CFile::modeCreate);
        a_pSection->GetLongBinary(a_pszName, &xFile);
        xFile.Close();
    }

    HBITMAP hbmp = (HBITMAP)LoadImage(AfxGetInstanceHandle(), pszFile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE|LR_DEFAULTSIZE);
    CFile::Remove(pszFile);

    CBitmapWnd bwnd;
    bwnd.DoModal(hbmp, a_pszName);
}

//*****************************************************************
void CExplorer::OnRunTest()
{
    int idx = m_ctlTestCases.GetCurSel();

    TEST_CASE *pTestCase = (TEST_CASE *)m_ctlTestCases.GetItemDataPtr(idx);

    if(pTestCase)
    {
        EnableWindow(FALSE);
        testing::runTest(pTestCase);
        EnableWindow(TRUE);
    }
}

//*****************************************************************
void CExplorer::OnRunAllTests()
{
     EnableWindow(FALSE);
     testing::runTest(NULL);
     EnableWindow(TRUE);
}
