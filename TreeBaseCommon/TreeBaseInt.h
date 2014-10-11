// TreeBaseInt.h
#if !defined(TREEBASEINT_H)
#define TREEBASEINT_H 

#include "TreeBase.h"
#include "TreeBaseInt.h"
#include "TreeBaseCommon.h"
#include <mapidefs.h>

#define KOBJNAME(s) "Global\\" s

#define WM_TB_THREAD_ATTACH  WM_APP + 0x1

#define TBVITYPE_NOTUSED     0 
#define TBVITYPE_RESERVED    1 
#define TBVITYPE_DELETED     2 

#define ITERATOR_END_BLOCK   0xFFFE
#define ITERATOR_END_ALL     0xFFFF

#define MAX_NAME            127

#define BLOB_SIZE_MAX      0x800000

typedef DWORD BPOINTER;

#define BP_PAGE(x) HIWORD(x)
#define BP_OFS(x)  LOWORD(x)

#define BP_MAKEBPTR(pg, ofs) MAKELONG(ofs, pg)

#define MAKE_PTR(type, base, ofs) (type)((BYTE*)(base) + ofs)

#pragma pack(1)

enum eAction{
    eActionNone = 0,
    eActionSet,
    eActionDel
};

typedef union _TBVALUE_ENTRY
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
    FILETIME          date;
    long              blob;
    BYTE              binary[1];
}TBVALUE_ENTRY; 

typedef struct _TBITEM_ENTRY
{
    BPOINTER bpNext;
    WORD     wSize;
    WORD     wOrder; 
    DWORD    dwHash; 
    BYTE     iName;  
    BYTE     iType;  
    BYTE     iValue;  
}TBITEM_ENTRY;

typedef struct _FETCH_VALUE_STRUCT
{
    long         ofsNext;
    TBITEM_ENTRY item;
}FETCH_VALUE_STRUCT;

typedef struct _ID_ENTRY
{
    DWORD dwHash;
    WORD  wOrder;
}ID_ENTRY;

typedef struct _ITERATED_ITEM_ENTRY
{
    WORD wSize;
    DWORD dwType;
    ID_ENTRY id;
    char szName[1];
}ITERATED_ITEM_ENTRY;

typedef struct _TB_BATCH_ENTRY
{
    long         ofsNext;
    eAction      action;
    long         cbSize;   
    TBITEM_ENTRY item;
}TB_BATCH_ENTRY;

#pragma pack()


#define TBITEM_NAME(entry)    (LPSTR)         ((BYTE*)(entry) + (entry)->iName)
#define TBITEM_TYPE(entry)   *(DWORD*)        ((BYTE*)(entry) + (entry)->iType)
#define TBITEM_VALUE(entry)   (TBVALUE_ENTRY*)((BYTE*)(entry) + (entry)->iValue)
#define TBITEM_FOOTER(entry) *(WORD*)         ((BYTE*)(entry) + (entry)->wSize - sizeof(WORD))

class TMemAlloc;
class TErrorInfo;

DWORD  __TBCOMMON_EXPORT__ HASH_ComputeHash(LPCSTR a_pszText);
void   __TBCOMMON_EXPORT__ COMMON_Initialize(TMemAlloc *a_pMalloc, TErrorInfo *a_pErrorInfo);
BOOL   __TBCOMMON_EXPORT__ COMMON_CheckValueName(LPCSTR a_pszName);
LPVOID __TBCOMMON_EXPORT__ COMMON_CreateItemEntry(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize, TMemAlloc *a_pMalloc = NULL);
LPCSTR __TBCOMMON_EXPORT__ COMMON_GetEntryPointName();
LPCSTR __TBCOMMON_EXPORT__ COMMON_GetGlobalMutexName();
DWORD  __TBCOMMON_EXPORT__ COMMON_PageSizeAdjustedToFibonacci();
DWORD  __TBCOMMON_EXPORT__ COMMON_GetFibonacciBaseSize();

class __TBCOMMON_EXPORT__ TMemAlloc
{
public:
    virtual LPVOID Alloc(DWORD a_cbSize);
    virtual void   Free(LPVOID a_pvData);
    virtual LPVOID ReAlloc(LPVOID a_pvData, DWORD a_cbSize);
    virtual DWORD  MemSize(LPVOID a_pvData);
};

class __TBCOMMON_EXPORT__ TErrorInfo
{
public:
    virtual void SetTaskErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)=0;
};

typedef struct
{
    DWORD    dwSize;
    FILETIME mTime;
}LBSTATUS;

typedef struct
{
    LPCSTR pszText;
    long   nLenText; 
}TB_TOKEN;

class __TBCOMMON_EXPORT__ TItemFilterData
{
    char      m_szBuff[128], m_szName[128];
    DWORD     m_dwTypeMask;
    TB_TOKEN *m_pTokenArray;
    long      m_nTokenCount;  
    BOOL      m_bIsFilter;
public:
	BOOL IsSuitable(LPCSTR a_pszName, DWORD a_dwType);
    TItemFilterData();
    ~TItemFilterData();
    BOOL PrepareFilter(TBASE_ITEM_FILTER *a_pFilter);
    inline BOOL IsFilter(){return m_bIsFilter; };
};


#endif //TREEBASEINT_H