#include "stdafx.h"
#include "value.h"
#include "Treebase.h"
#include "TreebaseInt.h"
#include "Tbcore.h"

static BOOL _ValidateData(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize);
static long _GetDataSize(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize);
static TBASE_VALUE* _AllocValue(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize, BOOL a_bValidate);


//********************************************************************
TBASE_VALUE* VALUE_ItemEntry2Value(TBITEM_ENTRY *a_pEntry)
{
    DWORD             dwType = TBITEM_TYPE(a_pEntry);
    long              cbSize = 0;  
    TBASE_VALUE_DATA *pData  = NULL;
    TBASE_VALUE_DATA  xData  = {0};
    TBVALUE_ENTRY    *pVal   = NULL;
    SYSTEMTIME        sysTime;

    //------------------------------------------------
    if (dwType==TBVTYPE_BINARY)
    {
        cbSize = a_pEntry->wSize - long(a_pEntry->iValue);
    }

    //------------------------------------------------
    pVal  = TBITEM_VALUE(a_pEntry);

    if (TBITEM_TYPE(a_pEntry)==TBVTYPE_DATE)
    {
        FileTimeToSystemTime(&pVal->date, &sysTime);
        pData = (TBASE_VALUE_DATA*)&sysTime;
    }
    else if (TBITEM_TYPE(a_pEntry)==TBVTYPE_LONGBINARY)
    {
        xData.blob.cbSize = 0;
        xData.blob.hData  = NULL;
        pData = &xData;
    }
    else
        pData = (TBASE_VALUE_DATA*)pVal;

    //------------------------------------------------
    return _AllocValue(dwType, pData, cbSize, FALSE);
}

//********************************************************************
static BOOL _ValidateData(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize)
{
    BOOL bValid = TRUE;
    long cbSize = 0; 

    switch (a_dwType)
    {
        case TBVTYPE_INTEGER:
        case TBVTYPE_FLOAT:  
            break; 
        case TBVTYPE_TEXT:    
            if (strlen(a_pData->text)>255)
                return FALSE;
            break;
        case TBVTYPE_BINARY:  
            if (a_cbSize>512)
                return FALSE;
            break;
        case TBVTYPE_DATE:        
        {
            SYSTEMTIME *pSysTime = &a_pData->date;

            bValid =           pSysTime->wDay>=1          && pSysTime->wDay<=31;
            bValid = bValid && pSysTime->wDayOfWeek>=0    && pSysTime->wDayOfWeek<7;
            bValid = bValid && pSysTime->wMonth>=1        && pSysTime->wMonth<=32;
            bValid = bValid && pSysTime->wYear>=0         && pSysTime->wMonth<=3000;
            bValid = bValid && pSysTime->wHour>=0         && pSysTime->wHour<24;
            bValid = bValid && pSysTime->wMinute>=0       && pSysTime->wMinute<60;
            bValid = bValid && pSysTime->wSecond>=0       && pSysTime->wSecond<60;
            bValid = bValid && pSysTime->wMilliseconds>=0 && pSysTime->wMilliseconds<1000;
            return bValid;                
        }
        case TBVTYPE_PTGROUP:     
            if (a_pData->ptGroup.count>32)
                return FALSE;
            break;
        case TBVTYPE_FPTGROUP:    
            if (a_pData->ptGroup.count>32)
                return FALSE;
            break;
        default:
            return FALSE;                
    }
    return TRUE;
}

//********************************************************************
static long _GetDataSize(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize)
{
    long cbSize = 0;

    switch (a_dwType)
    {
        case TBVTYPE_INTEGER:
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(long);
            break;
        case TBVTYPE_FLOAT:   
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(double);
            break;
        case TBVTYPE_TEXT:    
            cbSize = offsetof(TBASE_VALUE, data) + strlen(a_pData->text) + 1;
            if (cbSize>255)
                return 0;
            break;
        case TBVTYPE_RECT:        
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(RECT);
            break;
        case TBVTYPE_POINT:       
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(POINT);
            break;
        case TBVTYPE_FRECT:       
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(FLOAT_RECT);
            break;
        case TBVTYPE_FPOINT:      
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(FLOAT_POINT);
            break;
        case TBVTYPE_BINARY:  
            if (a_cbSize>512)
                return 0;
            cbSize = a_cbSize;
            break;
        case TBVTYPE_DATE:        
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(SYSTEMTIME);
            break;
        case TBVTYPE_CURRENCY:
            cbSize = offsetof(TBASE_VALUE, data) + sizeof(CURRENCY);
            break;
        case TBVTYPE_PTGROUP:     
            if (a_pData->ptGroup.count>32)
                return 0;
            cbSize = offsetof(TBASE_VALUE, data) + offsetof(TB_FPTGROUP_ENTRY, points) + a_pData->ptGroup.count * sizeof(POINT);
            break;
        case TBVTYPE_FPTGROUP:    
            if (a_pData->ptGroup.count>32)
                return 0;
            cbSize = offsetof(TBASE_VALUE, data) + offsetof(TB_FPTGROUP_ENTRY, points) + a_pData->ptGroup.count * sizeof(FLOAT_POINT);
            break;
    }
    return cbSize;
}

//********************************************************************
static TBASE_VALUE* _AllocValue(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize, BOOL a_bValidate)
{
    TBASE_VALUE* pValue = NULL;
    DWORD        cbSize = 0;
    TTaskInfo*   pTaskInfo = TASK_GetCurrent();
    
    if (pTaskInfo==NULL)
        return NULL;

    //-----------------------------------------------------------
    if (a_bValidate)
        if (!_ValidateData(a_dwType, a_pData, a_cbSize))
        {
            TASK_SetErrorInfo(TRERR_INVALID_DATA, NULL, NULL);
            return NULL;
        };
    cbSize = _GetDataSize(a_dwType, a_pData, a_cbSize);
    //-----------------------------------------------------------
    if (cbSize==0)
    {
        TASK_SetErrorInfo(TRERR_INVALID_TYPE, NULL, NULL);
        return NULL;
    }
    //-----------------------------------------------------------
    pValue = (TBASE_VALUE*)malloc(cbSize);
    pValue->dwType = a_dwType;
    pValue->cbSize = cbSize;
    pValue->dwRefCount = 1;

    // wype³nij...
    memcpy(&pValue->data, a_pData, cbSize-offsetof(TBASE_VALUE, data));

    pTaskInfo->m_setPointers.insert(pValue);
    return pValue;
}

//********************************************************************
TBASE_VALUE* WINAPI TBASE_AllocValue(DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize)
{
    return _AllocValue(a_dwType, a_pData, a_cbSize, TRUE);
}

//********************************************************************
BOOL WINAPI TBASE_ReAllocValue(TBASE_VALUE** a_ppValue, DWORD a_dwType, TBASE_VALUE_DATA *a_pData, DWORD a_cbSize)
{
    TBASE_VALUE* pValue = NULL;
    DWORD        cbSize = 0;
    TTaskInfo*   pTaskInfo = TASK_GetCurrent();
    TPtrIter     iter;     
    
    if (pTaskInfo==NULL)
        return NULL;

    //-----------------------------------------------------------
    if (IsBadWritePtr(a_ppValue, sizeof(TBASE_VALUE*)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    pValue = *a_ppValue;
    //----------------------------------------------------------
    iter = pTaskInfo->m_setPointers.find(pValue);
    if (iter==pTaskInfo->m_setPointers.end())
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //----------------------------------------------------------
    if (!_ValidateData(a_dwType, a_pData, a_cbSize))
    {
        TASK_SetErrorInfo(TRERR_INVALID_DATA, NULL, NULL);
        return NULL;
    };
    //-----------------------------------------------------------

    cbSize = _GetDataSize(a_dwType, a_pData, a_cbSize);
    //-----------------------------------------------------------
    if (cbSize==0)
    {
        TASK_SetErrorInfo(TRERR_INVALID_TYPE, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------------------------
    pValue = (TBASE_VALUE*)realloc(pValue, cbSize);
    pValue->dwType = a_dwType;
    pValue->cbSize = cbSize;

    // wype³nij...
    memcpy(&pValue->data, a_pData, cbSize-offsetof(TBASE_VALUE, data));

    *a_ppValue = pValue;
    *iter      = pValue;

    return TRUE;
}


//********************************************************************
BOOL WINAPI TBASE_FreeValue(TBASE_VALUE* a_pValue)
{
    TTaskInfo*   pTaskInfo = TASK_GetCurrent();
    TPtrIter     iter;     

    if (pTaskInfo==NULL)
    {
        return FALSE;
    }
    //----------------------------------------------------------
    iter = pTaskInfo->m_setPointers.find(a_pValue);
    if (iter==pTaskInfo->m_setPointers.end())
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //----------------------------------------------------------
    if ((--a_pValue->dwRefCount)==0)
    {
        pTaskInfo->m_setPointers.erase(iter);

        if (a_pValue->dwType==TBVTYPE_LONGBINARY)
        {
            if (a_pValue->data.blob.hData && a_pValue->data.blob.cbSize)
                GlobalFree(a_pValue->data.blob.hData);
        }
        free(a_pValue);
    }

    return TRUE;
}

//********************************************************************
BOOL WINAPI TBASE_AddValueRef(TBASE_VALUE* a_pValue)
{
    TTaskInfo*   pTaskInfo = TASK_GetCurrent();
    TPtrIter     iter;     

    if (pTaskInfo==NULL)
    {
        return FALSE;
    }
    //----------------------------------------------------------
    iter = pTaskInfo->m_setPointers.find(a_pValue);
    if (iter==pTaskInfo->m_setPointers.end())
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    a_pValue->dwRefCount++;
    return TRUE;
}
