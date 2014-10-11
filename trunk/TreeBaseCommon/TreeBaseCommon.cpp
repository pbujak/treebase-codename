// TreeBaseCommon.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "TreeBaseInt.h"
#include "TreeBasePriv.h"
#include <string.h>
#include <malloc.h>
#include <vector>

#define FOOTER_SIZE sizeof(WORD)
#define FIBONACCI_BASE_SIZE 24

static TMemAlloc  _Alloc;

TMemAlloc  *G_pMalloc    = &_Alloc;
TErrorInfo *G_pErrorInfo = NULL;

//************************************************************************
void SYS_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection)
{
    if (G_pErrorInfo)
        G_pErrorInfo->SetTaskErrorInfo(a_dwError, a_pszErrorItem, a_pszSection);
};


//************************************************************************
LPVOID TMemAlloc::Alloc(DWORD a_cbSize)
{
    return malloc(a_cbSize);
};

//************************************************************************
LPVOID TMemAlloc::ReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return realloc(a_pvData, a_cbSize);
};

//************************************************************************
void TMemAlloc::Free(LPVOID a_pvData)
{
    free(a_pvData);
};

//************************************************************************
DWORD TMemAlloc::MemSize(LPVOID a_pvData)
{
    return _msize(a_pvData);
};

//************************************************************************
TItemFilterData::TItemFilterData()
{
    m_pTokenArray = NULL;
    m_nTokenCount = 0;
    m_dwTypeMask  = 0;
    m_bIsFilter   = FALSE;
    memset(m_szBuff, 0, sizeof(m_szBuff));
    memset(m_szName, 0, sizeof(m_szName));
};

//************************************************************************
TItemFilterData::~TItemFilterData()
{
    if (m_pTokenArray)
        free(m_pTokenArray);
};

//************************************************************************
BOOL TItemFilterData::PrepareFilter(TBASE_ITEM_FILTER *a_pFilter)
{
    LPCSTR pszPattern = NULL;
    char*  pszText    = 0;    
    long   ctr=0, nCount=0, nLen=0;
    BYTE  *pbyTypes = NULL;  
    std::vector<TB_TOKEN> vecToken;
    TB_TOKEN              token  = {0};  
    BOOL                  bStart = TRUE;  
    char                  chEscape = 0;  

    m_bIsFilter = FALSE;
    if (a_pFilter==NULL)
        return FALSE;

    pszPattern = a_pFilter->szPattern;
    nCount     = a_pFilter->wTypeCount;
    pbyTypes   = a_pFilter->byTypes;
    chEscape   = a_pFilter->chEscape;
    strncpy(m_szBuff, pszPattern, 127);
    strupr(m_szBuff);

    if (nCount>0)
    {
        for(ctr=0; ctr<nCount; ctr++)
        {
            m_dwTypeMask |= (1 << DWORD(pbyTypes[ctr]));
        }
    }
    else
    {
        m_dwTypeMask = DWORD(-1);
    }

    pszText = m_szBuff;

    while (*pszText != 0)
    {
        if (*pszText==chEscape)
        {
            pszText++;
            token.nLenText++;
            if (*pszText==NULL)
                return FALSE;
            nLen = strlen(pszText);
            memmove(pszText-1, pszText, nLen+1);
            continue;
        }
        else
        {
            if (*pszText=='*' || *pszText=='?')
            {
                vecToken.push_back(token);
                token.pszText  = pszText;
                token.nLenText = 1;
                vecToken.push_back(token);
                bStart = TRUE;
            }
            else
            {
                if (bStart)
                {
                    token.pszText = pszText;
                    token.nLenText = 0;
                    bStart = FALSE;
                }
                token.nLenText++;
            }
        }
        pszText++;
    }
    if (!bStart)
    {
        if (token.pszText[0]!=0)
            vecToken.push_back(token);
    }
    nCount = vecToken.size();
    m_pTokenArray = (TB_TOKEN*)malloc(nCount * sizeof(TB_TOKEN));
    if (m_pTokenArray)
    {
        for (ctr=0; ctr<nCount; ctr++)
        {
            m_pTokenArray[ctr] = vecToken[ctr];
        }
        m_nTokenCount = nCount;
    }
    m_bIsFilter = TRUE;
    return TRUE;
};

//************************************************************************
BOOL TItemFilterData::IsSuitable(LPCSTR a_pszName, DWORD a_dwType)
{
    DWORD dwType1 = 0;
    long  ctr     = 0;
    char* pszName = NULL;  
    TB_TOKEN *pToken = NULL;
    
    if (!m_bIsFilter)
        return TRUE;

    strncpy(m_szName, a_pszName, 127);
    strupr(m_szName);
    pszName = m_szName;

    dwType1 = 1 << a_dwType;

    if ((dwType1 & m_dwTypeMask) == 0)
        return FALSE;

    for (ctr=0; ctr<m_nTokenCount; ctr++)
    {
        pToken = &m_pTokenArray[ctr];
        if (pToken->pszText[0]=='?')
        {
            if (*pszName==0)
                return FALSE;
            pszName++;
        }
        else if(pToken->pszText[0]=='*')
        {
            if (ctr==(m_nTokenCount-1))
                return TRUE;
            pToken = &m_pTokenArray[ctr+1];
            pszName = strstr(pszName, pToken->pszText);
            if (pszName==NULL)
                return FALSE;
        }
        else
        {
            if (strncmp(pszName, pToken->pszText, pToken->nLenText)!=0)
                return FALSE;
            pszName += pToken->nLenText;
        }
    }
    return (pszName[0]==0);
}

//************************************************************************
static int AdjustToFibonacciSize(int a_cbSize)
{
    int cbBaseSize = FIBONACCI_BASE_SIZE;
    int cbPrevBaseSize = 0;

    while((a_cbSize + FOOTER_SIZE) > cbBaseSize)
    {
        int cbNewBaseSize = cbBaseSize + cbPrevBaseSize;
        cbPrevBaseSize = cbBaseSize;
        cbBaseSize = cbNewBaseSize;
    }
    return cbBaseSize;
}

//************************************************************************
DWORD __TBCOMMON_EXPORT__ COMMON_PageSizeAdjustedToFibonacci()
{
    return PAGESIZE - (PAGESIZE % FIBONACCI_BASE_SIZE);
}

//************************************************************************
DWORD __TBCOMMON_EXPORT__ COMMON_GetFibonacciBaseSize()
{
    return FIBONACCI_BASE_SIZE;
}

//************************************************************************
DWORD __TBCOMMON_EXPORT__ HASH_ComputeHash(LPCSTR a_pszText)
{
	DWORD  dwHash = 0;
    long   nLen   = 0; 
    long   ctr    = 0;
    LPSTR  pszKey = NULL;

    if (!a_pszText)
        return 0;

    nLen = strlen(a_pszText);

    pszKey = (LPSTR)SYS_MemAlloc(nLen+1);
    if (pszKey==NULL)
        return 0;

    strcpy(pszKey, a_pszText);
    strupr(pszKey);

	while (pszKey[ctr])
		dwHash = (dwHash<<5) + dwHash + pszKey[ctr++];

    SYS_MemFree(pszKey);

	return dwHash;
};

//**************************************************************
BOOL __TBCOMMON_EXPORT__ COMMON_CheckValueName(LPCSTR a_pszName)
{
    static LPCSTR pszForbidden = "\\*/";
    LPCSTR pszChar = pszForbidden;
    BOOL   bInRange = FALSE; 
    
    if (strlen(a_pszName)>MAX_NAME)
        return FALSE;

    bInRange = (a_pszName[0]>='A' && a_pszName[0]<='Z') || (a_pszName[0]>='a' && a_pszName[0]<='z');
    if (!bInRange)
        return FALSE;

    while (*pszChar)
    {
        if (strchr(a_pszName, *(pszChar++)))
            return FALSE;
    }
    return TRUE;
}

//**********************************************************
LPVOID __TBCOMMON_EXPORT__ COMMON_CreateItemEntry(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize, TMemAlloc *a_pMalloc)
{
    long          nNameLen  = a_pszName ? strlen(a_pszName)+1 : 0;  
    long          nOffset   = 0;
    long          cbBlkSize = 0;
    TBITEM_ENTRY *pNewItem  = NULL;
    DWORD         dwHash    = HASH_ComputeHash(a_pszName);
    long          cbSize    = a_cbSize;

    if(a_pMalloc == NULL)
        a_pMalloc = G_pMalloc;

    switch(a_dwType)
    {
        case TBVTYPE_INTEGER    :
            cbSize = sizeof(long);
            break;
        case TBVTYPE_FLOAT      :
            cbSize = sizeof(double);
            break;
        case TBVTYPE_RECT       :
            cbSize = sizeof(RECT);
            break;
        case TBVTYPE_POINT      :
            cbSize = sizeof(POINT);
            break;
        case TBVTYPE_FRECT       :
            cbSize = sizeof(FLOAT_RECT);
            break;
        case TBVTYPE_FPOINT      :
            cbSize = sizeof(FLOAT_POINT);
            break;
        case TBVTYPE_DATE       :
            cbSize = sizeof(FILETIME);
            break;
        case TBVTYPE_PTGROUP:
            cbSize = offsetof(TB_PTGROUP_ENTRY, points) + a_pValue->ptGroup.count * sizeof(POINT);
            break;
        case TBVTYPE_FPTGROUP:
            cbSize = offsetof(TB_FPTGROUP_ENTRY, points) + a_pValue->ptGroup.count * sizeof(FLOAT_POINT);
            break;
        case TBVTYPE_CURRENCY   :
            cbSize = sizeof(CURRENCY);
            break;
        case TBVTYPE_TEXT   :
            cbSize = strlen(a_pValue->text) + 1;
            break;
        case TBVTYPE_SECTION:
        case TBVTYPE_LONGBINARY:
            cbSize = sizeof(long);
            break;
    }

    cbBlkSize = sizeof(TBITEM_ENTRY) + nNameLen + sizeof(DWORD) + cbSize;

    cbBlkSize = AdjustToFibonacciSize(cbBlkSize);
    pNewItem  = (TBITEM_ENTRY *)a_pMalloc->Alloc(cbBlkSize);

    if (pNewItem==NULL)
        return FALSE;

    // fill new item
    nOffset = sizeof(TBITEM_ENTRY);
    pNewItem->dwHash = dwHash;
    pNewItem->wSize  = (WORD)cbBlkSize;
    pNewItem->iName  = BYTE(nOffset);
    pNewItem->iType  = BYTE(nOffset += nNameLen);
    pNewItem->iValue = BYTE(nOffset += sizeof(DWORD));

    strcpy(TBITEM_NAME(pNewItem), a_pszName); 
    TBITEM_TYPE(pNewItem) = a_dwType;
    memcpy(TBITEM_VALUE(pNewItem), a_pValue, cbSize);
    return pNewItem;
}


//************************************************************************
void __TBCOMMON_EXPORT__ COMMON_Initialize(TMemAlloc *a_pMalloc, TErrorInfo *a_pErrorInfo)
{
    if (a_pMalloc)
        G_pMalloc = a_pMalloc;
    if (a_pErrorInfo)
        G_pErrorInfo = a_pErrorInfo;

};

//************************************************************************
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

