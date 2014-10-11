// TreeBase.h
#if !defined(TREEBASE_H)
#define TREEBASE_H

#include <objidl.h>

#define TBVTYPE_INTEGER         1 
#define TBVTYPE_FLOAT           2
#define TBVTYPE_TEXT            3
#define TBVTYPE_RECT            4 
#define TBVTYPE_POINT           5
#define TBVTYPE_FRECT           6 
#define TBVTYPE_FPOINT          7
#define TBVTYPE_BINARY          8
#define TBVTYPE_DATE            9
#define TBVTYPE_CURRENCY       10
#define TBVTYPE_PTGROUP        11
#define TBVTYPE_FPTGROUP       12
#define TBVTYPE_LONGBINARY     32
#define TBVTYPE_SECTION        64

typedef struct _HTHANDLE
{
}*HTBHANDLE;

typedef struct _HTBSECTION
{
    HTBHANDLE handle;
}*HTBSECTION;

typedef struct _HTBSTREAMIN
{
    HTBHANDLE handle;
}*HTBSTREAMIN;

typedef struct _HTBSTREAMOUT
{
    HTBHANDLE handle;
}*HTBSTREAMOUT;

typedef struct _HTBACL
{
    HTBHANDLE handle;
}*HTBACL;


#define TBSECTION_ROOT (HTBSECTION)(-1)

#define DELHINT_SIMPLE      0
#define DELHINT_LONGBINARY  1
#define DELHINT_AUTO        2

typedef struct _FLOAT_RECT
{
    double fX, fY, fcX, fcY;
}FLOAT_RECT;

typedef struct _FLOAT_POINT
{
    double fX, fY;
}FLOAT_POINT;

typedef struct _TB_PTGROUP_ENTRY
{
    WORD  count;
    WORD  type;
    POINT points[1];
}TB_PTGROUP_ENTRY;

typedef struct _TB_FPTGROUP_ENTRY
{
    WORD        count;
    WORD        type;
    FLOAT_POINT points[1];
}TB_FPTGROUP_ENTRY;

typedef struct _TB_LONGBINARY
{
    long    cbSize;
    HGLOBAL hData;
}TB_LONGBINARY;

typedef struct _TB_BINARY
{
    long cbSize;
    BYTE data[1];
}TB_BINARY;


typedef union _TBASE_VALUE_DATA
{
    RECT              rect;
    POINT             point;
    FLOAT_RECT        frect;
    FLOAT_POINT       fpoint;
    TB_PTGROUP_ENTRY  ptGroup;
    TB_FPTGROUP_ENTRY fptGroup;
    long              int32;
    double            float64;
    char              text[1]; 
    CURRENCY          currency;
    SYSTEMTIME        date;
    BYTE              binary[1];
    TB_LONGBINARY     blob;
}TBASE_VALUE_DATA;

typedef struct _TBASE_VALUE
{
    DWORD            cbSize;
    DWORD            dwType;
    DWORD            dwRefCount;
    TBASE_VALUE_DATA data;
}TBASE_VALUE;

typedef struct _TBASE_ERROR_INFO
{
    DWORD  dwErrorCode;
    char   szErrorItem[128];
    char   szErrorSection[512];
    char   szDescription[256];
}TBASE_ERROR_INFO;

typedef BOOL (CALLBACK *TBASE_EXPORT_STREAM_CALLBACK)(UINT a_uCode, DWORD a_cbBytes, LPVOID a_pvBuff, LPVOID a_pvCookie);
typedef BOOL (CALLBACK *BLOB_GETDATA_CALLBACK)(LPVOID a_pvParam, DWORD *a_pdwSize, LPVOID a_pvBuff);
typedef BOOL (CALLBACK *BLOB_PUTDATA_CALLBACK)(LPVOID a_pvParam, DWORD a_dwSize, LPVOID a_pvBuff);

typedef struct _TBASE_EXPORT_STREAM
{
    LPVOID                       pvCookie;
    TBASE_EXPORT_STREAM_CALLBACK pfnCallBack;
}TBASE_EXPORT_STREAM;

typedef struct _TBASE_ITEM_FILTER
{
    char szPattern[128];
    char chEscape;
    WORD wTypeCount;   
    BYTE byTypes[20];
}TBASE_ITEM_FILTER;

typedef struct _TBSECURITY_ATTRIBUTES
{
    DWORD dwAccessRightsOwner;
    DWORD dwAccessRightsAdmins;
    DWORD dwAccessRightsAll;
    HTBACL hWhiteList;
    HTBACL hBlackList;
}TBSECURITY_ATTRIBUTES;

typedef struct _TBSECURITY_INFORMATION
{
    enum Flags
    {
        eNONE = 0x0,
        eOWNER = 0x1,
        ePROTECTION_DOMAIN = 0x2,
        eATTRIBUTES = 0x4
    };
}TBSECURITY_INFORMATION;

typedef struct _TBSECURITY_OPERATION
{
    enum Enum
    {
        eREPLACE = 0x0,
        eADD,
        eREMOVE
    };
}TBSECURITY_OPERATION;

typedef struct _TBSECURITY_TARGET
{
    enum Enum
    {
        eNOTHING = 0x0,
        eSECTION,
        ePROTECTION_DOMAIN
    };
}TBSECURITY_TARGET;


#define TBATTR_NOVALUES       0x1

#define TBEXP_STREAM_READ     0x00
#define TBEXP_STREAM_WRITE    0x01

#define TBACCESS_BROWSE      0x001
#define TBACCESS_GETVALUE    0x002
#define TBACCESS_EXPORT      0x004
#define TBACCESS_MODIFY      0x008  
#define TBACCESS_OPENDYNAMIC 0x010 
#define TBACCESS_READ        (TBACCESS_BROWSE | TBACCESS_GETVALUE | TBACCESS_EXPORT)
#define TBACCESS_WRITE       (TBACCESS_MODIFY | TBACCESS_OPENDYNAMIC)
#define TBACCESS_LINK        0x020
#define TBACCESS_DELETE      0x040  // includes BLKMASK_ALLOWREAD
#define TBACCESS_SETSTATUS   0x080

#define TBACCESS_READWRITE   (TBACCESS_READ | TBACCESS_WRITE)
#define TBACCESS_EXECUTE     (TBACCESS_LINK | TBACCESS_DELETE | TBACCESS_SETSTATUS)
#define TBACCESS_ALL         (TBACCESS_READWRITE | TBACCESS_EXECUTE)
#define TBACCESS_NO_GETVALUE (TBACCESS_ALL & ~TBACCESS_GETVALUE)

#define TBACCESS_ADMIN_MINIMAL (TBACCESS_DELETE | TBACCESS_EXPORT | TBACCESS_SETSTATUS)

#define TBOPEN_MODE_DEFAULT   0
#define TBOPEN_MODE_SNAPSHOT  1
#define TBOPEN_MODE_DYNAMIC   2

#define TR_OK                         0
#define TRERR_INVALID_NAME            1
#define TRERR_ITEM_NOT_FOUND          2
#define TRERR_PATH_NOT_FOUND          3
#define TRERR_ALREADY_EXIST           4
#define TRERR_ACCESS_DENIED           5
#define TRERR_SHARING_DENIED          6
#define TRERR_ITEM_ALREADY_EXIST      8
#define TRERR_ILLEGAL_OPERATION       9
#define TRERR_CANNOT_CREATE_SECTION  10
#define TRERR_SECTION_NOT_EMPTY      11
#define TRERR_INVALID_PARAMETER      12
#define TRERR_OUT_OF_DATA            13
#define TRERR_DISK_FULL              14
#define TRERR_CANNOT_CREATE_BLOB     15
#define TRERR_INVALID_TYPE           16
#define TRERR_INVALID_DATA           17
#define TRERR_BUS_ERROR              18
#define TRERR_LABEL_ALREADY_USED     19 
#define TRERR_CANNOT_SET_LABEL       20 
#define TRERR_SECTION_NOT_MOUNTED    21
#define TRERR_STORAGE_FAILURE        22
#define TRERR_SERVICE_UNAVAILABLE    23
#define TRERR_HANDLE_BUSY            24
#define TRERR_INVALID_SECURITY        25
#define TRERR_INVALID_USERID          26
#define TRERR_CANNOT_REGISTER_USER    27
#define TRERR_CHANGING_CURRENT_DOMAIN 28
#define TRERR_SYSTEM_ALARM          100

extern "C"
{
    void _xxx_test();
    BOOL          WINAPI  TBASE_Flush              ();
    HTBSECTION    WINAPI  TBASE_CreateSection      (HTBSECTION a_hParent, LPCSTR a_pszName, DWORD a_dwAttrs, TBSECURITY_ATTRIBUTES *a_pSecAttrs);
    HTBSECTION    WINAPI  TBASE_OpenSection        (HTBSECTION a_hBase, LPCSTR a_pszPath, DWORD a_dwOpenMode);
    BOOL          WINAPI  TBASE_CloseSection       (HTBSECTION a_hSection);
    BOOL          WINAPI  TBASE_DeleteSection      (HTBSECTION a_hParent, LPCSTR a_pszName);
    BOOL          WINAPI  TBASE_RenameSection      (HTBSECTION a_hParent, LPCSTR a_pszOldName, LPCSTR a_pszNewName);
    BOOL          WINAPI  TBASE_CreateSectionLink  (HTBSECTION a_hSourceBase, HTBSECTION a_hTargetParent, LPCSTR a_pszSourcePath, LPCSTR a_pszTargetName);
    BOOL          WINAPI  TBASE_GetSectionLinkCount(HTBSECTION a_hSection, DWORD *a_pdwLinkCount);
    BOOL          WINAPI  TBASE_GetFirstItem       (HTBSECTION a_hSection, TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType);
    BOOL          WINAPI  TBASE_GetNextItem        (HTBSECTION a_hSection, LPSTR a_pszBuffer, DWORD *a_pdwType);
    TBASE_VALUE*  WINAPI  TBASE_AllocValue         (DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize);
    BOOL          WINAPI  TBASE_ReAllocValue       (TBASE_VALUE** a_ppValue, DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize);
    BOOL          WINAPI  TBASE_AddValueRef        (TBASE_VALUE* a_pValue);
    BOOL          WINAPI  TBASE_FreeValue          (TBASE_VALUE* a_pValue);
    BOOL          WINAPI  TBASE_GetValue           (HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE **a_ppValue);
    BOOL          WINAPI  TBASE_SetValue           (HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE *a_pValue);
    BOOL          WINAPI  TBASE_Edit               (HTBSECTION a_hSection);
    BOOL          WINAPI  TBASE_Update             (HTBSECTION a_hSection);
    BOOL          WINAPI  TBASE_CancelUpdate       (HTBSECTION a_hSection);
    BOOL          WINAPI  TBASE_DeleteValue        (HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwHint);
    BOOL          WINAPI  TBASE_SectionExists      (HTBSECTION a_hBase,   LPCSTR a_pszPath,  BOOL *a_pbExist);
    BOOL          WINAPI  TBASE_ValueExists        (HTBSECTION a_hParent, LPCSTR a_pszValue, BOOL *a_pbExist);
    BOOL          WINAPI  TBASE_GetLastErrorInfo   (TBASE_ERROR_INFO *a_pErrorInfo);
    BOOL          WINAPI  TBASE_LinkLongBinary     (HTBSECTION a_hSource, LPCSTR a_pszName, HTBSECTION a_hTarget, LPCSTR a_pszLinkName);
    BOOL          WINAPI  TBASE_GetSectionName     (HTBSECTION a_hSection, BOOL a_bFull, LPSTR a_pszBuffer);
    BOOL          WINAPI  TBASE_GetSectionLabel    (HTBSECTION a_hSection, LPSTR a_pszLabel);
    BOOL          WINAPI  TBASE_SetSectionLabel    (HTBSECTION a_hSection, LPCSTR a_pszLabel);
    BOOL          WINAPI  TBASE_DeleteSectionLabel (HTBSECTION a_hSection);
    BOOL          WINAPI  TBASE_GetSectionAttributes(HTBSECTION a_hSection, DWORD *a_pdwAttrs);
    BOOL          WINAPI  TBASE_GetSectionAccess    (HTBSECTION a_hSection, DWORD *a_pdwAccess);
    BOOL          WINAPI  TBASE_ExportSection       (HTBSECTION a_hSection, TBASE_EXPORT_STREAM *a_pStream);
    BOOL          WINAPI  TBASE_ImportSection       (HTBSECTION a_hSection, TBASE_EXPORT_STREAM *a_pStream, BOOL a_bShareLabels);
    BOOL          WINAPI  TBASE_WriteLongBinary     (HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_GETDATA_CALLBACK a_pfnCallback, LPVOID a_pvCallbackParam);
    BOOL          WINAPI  TBASE_ReadLongBinary      (HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_PUTDATA_CALLBACK a_pfnCallback, LPVOID a_pvCallbackParam);

    DWORD         WINAPI  TBASE_GetData             (HTBSTREAMIN  a_hStream, void* a_pvBuff, DWORD a_nCount);
    DWORD         WINAPI  TBASE_PutData             (HTBSTREAMOUT a_hStream, void* a_pvBuff, DWORD a_nCount);
    BOOL          WINAPI  TBASE_CloseInputStream    (HTBSTREAMIN  a_hStream); 
    BOOL          WINAPI  TBASE_CloseOutputStream   (HTBSTREAMOUT a_hStream);

    HTBACL        WINAPI  TBASE_AclCreate           ();
    BOOL          WINAPI  TBASE_AclDestroy          (HTBACL a_hAcl);
    BOOL          WINAPI  TBASE_AclResetAccessRights(HTBACL a_hAcl, PSID a_sid);
    BOOL          WINAPI  TBASE_AclSetAccessRights  (HTBACL a_hAcl, PSID a_sid, DWORD a_dwAccessRights);
    BOOL          WINAPI  TBASE_AclGetAccessRights  (HTBACL a_hAcl, PSID a_sid, DWORD *a_pdwAccessRights);
    int           WINAPI  TBASE_AclGetCount         (HTBACL a_hAcl);
    BOOL          WINAPI  TBASE_AclGetEntry         (HTBACL a_hAcl, DWORD a_dwIndex, PSID *a_psid, DWORD *a_pdwAccessRights);

    BOOL          WINAPI  TBASE_GetSectionSecurity  (HTBSECTION a_hParent, 
                                                     LPCSTR a_pszName, 
                                                     DWORD a_nSecurityInformationFlags, 
                                                     PSID* a_ppOwnerSid,
                                                     GUID* a_pProtectionDomain,
                                                     TBSECURITY_ATTRIBUTES *a_pSecAttrs);

    BOOL          WINAPI  TBASE_SetSectionSecurity  (HTBSECTION a_hParent, 
                                                     LPCSTR a_pszName,
                                                     TBSECURITY_TARGET::Enum a_target,
                                                     TBSECURITY_OPERATION::Enum a_operation,
                                                     TBSECURITY_ATTRIBUTES *a_pSecAttrs);


};

#endif //TREEBASE_H
