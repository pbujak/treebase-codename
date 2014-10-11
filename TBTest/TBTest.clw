; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CBitmapWnd
LastTemplate=generic CWnd
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "TBTest.h"

ClassCount=6
Class1=CTBTestApp
Class2=CTBTestDlg

ResourceCount=7
Resource2=IDD_TBTEST_DIALOG
Resource3=IDD_TBTEST_DIALOG (English (U.S.))
Resource1=IDR_MAINFRAME
Class3=CEditSec
Resource4=IDD_VALEDIT
Class4=CExplorer
Resource5=IDD_EDITSEC
Resource6=IDR_LISTMENU
Class5=CValEdit
Class6=CBitmapWnd
Resource7=IDD_EXPLORER

[CLS:CTBTestApp]
Type=0
HeaderFile=TBTest.h
ImplementationFile=TBTest.cpp
Filter=N
LastObject=CTBTestApp

[CLS:CTBTestDlg]
Type=0
HeaderFile=TBTestDlg.h
ImplementationFile=TBTestDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC



[DLG:IDD_TBTEST_DIALOG]
Type=1
ControlCount=3
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,static,1342308352
Class=CTBTestDlg

[DLG:IDD_TBTEST_DIALOG (English (U.S.))]
Type=1
Class=CTBTestDlg
ControlCount=5
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_LIST1,SysListView32,1342242829
Control4=IDC_NEW,button,1342242816
Control5=IDC_DELETE,button,1342242816

[DLG:IDD_EDITSEC]
Type=1
Class=CEditSec
ControlCount=8
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_NAME,edit,1350631552
Control4=IDC_STATIC,static,1342308352
Control5=IDC_ADDRESS,edit,1350631552
Control6=IDC_STATIC,static,1342308352
Control7=IDC_AGE,edit,1350639744
Control8=IDC_STATIC,static,1342308352

[CLS:CEditSec]
Type=0
HeaderFile=EditSec.h
ImplementationFile=EditSec.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=CEditSec

[DLG:IDD_EXPLORER]
Type=1
Class=CExplorer
ControlCount=12
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_LIST,SysListView32,1350631937
Control4=IDC_EDIT,edit,1342244992
Control5=IDC_BACK,button,1342242816
Control6=IDC_NEW_SECTION,button,1342242816
Control7=IDC_DELETE,button,1342242816
Control8=IDC_LABEL,edit,1350631552
Control9=IDC_STATIC,static,1342308352
Control10=IDC_DELETE_LABEL,button,1342242816
Control11=IDC_UPDATE,button,1342242816
Control12=IDC_ROLLBACK,button,1342242816

[CLS:CExplorer]
Type=0
HeaderFile=Explorer.h
ImplementationFile=Explorer.cpp
BaseClass=CDialog
Filter=D
LastObject=CExplorer
VirtualFilter=dWC

[MNU:IDR_LISTMENU]
Type=1
Class=?
Command1=ID_DELETE
Command2=ID_NEWSECTION
Command3=ID_EDIT_COPY
Command4=ID_EDIT_PASTE
Command5=ID_EDIT_VALUE
Command6=ID_ADD_VALUE
Command7=ID_ADD_IMAGE
CommandCount=7

[DLG:IDD_VALEDIT]
Type=1
Class=CValEdit
ControlCount=32
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC,button,1342177287
Control4=IDC_TEXT,edit,1350631552
Control5=IDC_INTEGER,edit,1350631552
Control6=IDC_FLOAT,edit,1350631552
Control7=IDC_STATIC,static,1342308352
Control8=IDC_STATIC,static,1342308352
Control9=IDC_STATIC,static,1342308352
Control10=IDC_STATIC,button,1342177287
Control11=IDC_POINT_X,edit,1350631552
Control12=IDC_POINT_Y,edit,1350631552
Control13=IDC_STATIC,static,1342308352
Control14=IDC_STATIC,static,1342308352
Control15=IDC_STATIC,button,1342177287
Control16=IDC_RECT_TOP,edit,1350631552
Control17=IDC_RECT_BOTTOM,edit,1350631552
Control18=IDC_RECT_RIGHT,edit,1350631552
Control19=IDC_RECT_LEFT,edit,1350631552
Control20=IDC_STATIC,button,1342177287
Control21=IDC_FPOINT_X,edit,1350631552
Control22=IDC_FPOINT_Y,edit,1350631552
Control23=IDC_STATIC,static,1342308352
Control24=IDC_STATIC,static,1342308352
Control25=IDC_STATIC,button,1342177287
Control26=IDC_FRECT_TOP,edit,1350631552
Control27=IDC_FRECT_BOTTOM,edit,1350631552
Control28=IDC_FRECT_RIGHT,edit,1350631552
Control29=IDC_FRECT_LEFT,edit,1350631552
Control30=IDC_VALTYPE,combobox,1344339970
Control31=IDC_VALNAME,edit,1350631552
Control32=IDC_STATIC,static,1342308352

[CLS:CValEdit]
Type=0
HeaderFile=ValEdit.h
ImplementationFile=ValEdit.cpp
BaseClass=CDialog
Filter=D
LastObject=CValEdit
VirtualFilter=dWC

[CLS:CBitmapWnd]
Type=0
HeaderFile=BitmapWnd.h
ImplementationFile=BitmapWnd.cpp
BaseClass=CWnd
Filter=W
VirtualFilter=WC

