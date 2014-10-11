#include "stdafx.h"
#include "tbcore.h"
#include "lpccodes.h"
#include "blob.h"
#include "acl.h"
#include "Shared/TAbstractThreadLocal.h"
#include <vector>

extern TDllMemAlloc  G_malloc; 

static TThreadLocal< std::vector<BYTE> > s_lastOwnerSid;

//***************************************************
HTBSECTION UTIL_GetSectionFromOutput(TTaskInfo *a_pTask, TCallParams &a_outParams)
{
    HTBSECTION hNewSection  = (HTBSECTION)a_outParams.GetHandle(SVP_NEW_HANDLE);
    LPCSTR    pszName      =            a_outParams.GetText  (SVP_PATH);
    DWORD     dwAttrs      = (DWORD)    a_outParams.GetLong  (SVP_ATTRIBUTES);
    BOOL      bReadOnly    = (BOOL)     a_outParams.GetLong  (SVP_READONLY);
    BOOL      bCanGetValue = (BOOL)     a_outParams.GetLong  (SVP_CANGETVALUE);

    TSection *pSection = a_pTask->AddNewSection(hNewSection, pszName, dwAttrs, bReadOnly);
    if(pSection)
    {
        pSection->m_bCanGetValue = bCanGetValue;
    }
    return hNewSection;
}

//***************************************************
BOOL UTIL_SerializeSecurity(TBSECURITY_ATTRIBUTES *a_pSecAttrs, TCallParams &a_inParams)
{
    if(a_pSecAttrs == NULL)
        return TRUE;

    if(IsBadReadPtr(a_pSecAttrs, sizeof(TBSECURITY_ATTRIBUTES)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }

    a_inParams.SetBufferPointer(SVP_SECURITY_ATTRIBUTES_BASE, a_pSecAttrs, sizeof(TBSECURITY_ATTRIBUTES));

    if(!ACL_SerializeAcl(a_pSecAttrs->hBlackList, a_inParams, SVP_SECURITY_ATTRIBUTES_BLACK_LIST))
        return FALSE;

    if(!ACL_SerializeAcl(a_pSecAttrs->hWhiteList, a_inParams, SVP_SECURITY_ATTRIBUTES_WHITE_LIST))
        return FALSE;

    return TRUE;
}

//***************************************************
void UTIL_DeserializeSecurity(TBSECURITY_ATTRIBUTES *a_pSecAttrs, TCallParams &a_outParams)
{
    void *pvBuff = NULL;
    long cbSize = 0;

    if(!a_outParams.GetBuffer(SVP_SECURITY_ATTRIBUTES_BASE, &pvBuff, cbSize) || 
        cbSize != sizeof(TBSECURITY_ATTRIBUTES))
    {
        return;
    }

    memcpy(a_pSecAttrs, pvBuff, sizeof(TBSECURITY_ATTRIBUTES));

    a_pSecAttrs->hWhiteList = ACL_DeserializeAcl(a_outParams, SVP_SECURITY_ATTRIBUTES_WHITE_LIST);
    a_pSecAttrs->hBlackList = ACL_DeserializeAcl(a_outParams, SVP_SECURITY_ATTRIBUTES_BLACK_LIST);
}

//***************************************************
HTBSECTION WINAPI TBASE_CreateSection(HTBSECTION a_hParent, LPCSTR a_pszName, DWORD a_dwAttrs, TBSECURITY_ATTRIBUTES *a_pSecAttrs)
{
    TCallParams inParams(SVF_CREATE_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return NULL;
    }
    //-----------------------------------------
    if(!TBASE_Update(a_hParent))
        return NULL;

    //-----------------------------------------
    inParams.SetHandle      (SVP_HPARENT,     (HTBHANDLE)a_hParent);
    inParams.SetTextPointer (SVP_NAME,        a_pszName);
    inParams.SetLong        (SVP_ATTRIBUTES,  a_dwAttrs);

    if(!UTIL_SerializeSecurity(a_pSecAttrs, inParams))
        return NULL;

    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    return UTIL_GetSectionFromOutput(pTask, outParams);
}

//***************************************************
HTBSECTION WINAPI TBASE_OpenSection(HTBSECTION a_hBase, LPCSTR a_pszPath, DWORD a_dwOpenMode)
{
    TCallParams inParams(SVF_OPEN_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (   IsBadStringPtr(a_pszPath, 512) 
        || (   a_dwOpenMode != TBOPEN_MODE_DEFAULT
            && a_dwOpenMode != TBOPEN_MODE_SNAPSHOT 
            && a_dwOpenMode != TBOPEN_MODE_DYNAMIC
            ) )
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return NULL;
    }
    //-----------------------------------------

    inParams.SetLong        (SVP_OPEN_MODE, a_dwOpenMode);
    inParams.SetHandle      (SVP_HBASE,   (HTBHANDLE)a_hBase);
    inParams.SetTextPointer (SVP_PATH,  a_pszPath);
    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    return UTIL_GetSectionFromOutput(pTask, outParams);
}

//***************************************************
BOOL WINAPI TBASE_CloseSection(HTBSECTION a_hSection)
{
    TCallParams inParams(SVF_CLOSE_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    //-----------------------------------------
    if (pTask==NULL)
        return NULL;
    //-----------------------------------------
    if (a_hSection==TBSECTION_ROOT)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, "", "\\");
        return FALSE;
    }

    //-----------------------------------------
    inParams.SetHandle      (SVP_HSECTION, (HTBHANDLE)a_hSection);
    bRetVal = pTask->CallServer(inParams, outParams);
    if (bRetVal)
    {
        pTask->RemoveSection(a_hSection);
    }
    return bRetVal;
};

//***************************************************
BOOL WINAPI TBASE_DeleteSection(HTBSECTION a_hParent, LPCSTR a_pszName)
{
    TCallParams inParams(SVF_DELETE_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hParent))
        return NULL;
    //-----------------------------------------
    inParams.SetHandle     (SVP_HPARENT, (HTBHANDLE)a_hParent);
    inParams.SetTextPointer(SVP_NAME,    a_pszName); 
    bRetVal = pTask->CallServer(inParams, outParams);
    //-----------------------------------------
    if (bRetVal)
    {
        SECTION_DeleteFromCache(a_hParent, a_pszName);
    }
    return bRetVal;
};

//***************************************************
BOOL WINAPI TBASE_RenameSection(HTBSECTION a_hParent, LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    TCallParams inParams(SVF_RENAME_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (IsBadStringPtr(a_pszOldName, 128) || IsBadStringPtr(a_pszNewName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hParent))
        return NULL;
    //-----------------------------------------
    inParams.SetHandle      (SVP_HPARENT, (HTBHANDLE)a_hParent);
    inParams.SetTextPointer (SVP_OLDNAME, a_pszOldName); 
    inParams.SetTextPointer (SVP_NEWNAME, a_pszNewName); 

    bRetVal = pTask->CallServer(inParams, outParams);
    //-----------------------------------------
    if (bRetVal)
    {
        SECTION_DeleteFromCache(a_hParent, a_pszOldName);
    }
    return bRetVal;
};

//****************************************************************************************
BOOL WINAPI TBASE_CreateSectionLink(HTBSECTION a_hSourceBase, HTBSECTION a_hTargetParent, LPCSTR a_pszSourcePath, LPCSTR a_pszTargetName)
{
    TCallParams inParams(SVF_CREATE_SECTION_LINK);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    //-----------------------------------------
    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (IsBadStringPtr(a_pszSourcePath, 512) || IsBadStringPtr(a_pszTargetName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hTargetParent))
        return NULL;
    //-----------------------------------------
    inParams.SetHandle      (SVP_HSOURCE_BASE,    (HTBHANDLE)a_hSourceBase);
    inParams.SetHandle      (SVP_HTARGET_SECTION, (HTBHANDLE)a_hTargetParent);
    inParams.SetTextPointer (SVP_SOURCE_PATH,     a_pszSourcePath);
    inParams.SetTextPointer (SVP_LINKNAME,        a_pszTargetName);

    return pTask->CallServer(inParams, outParams);
};

//****************************************************************************************
BOOL WINAPI TBASE_GetFirstItem(HTBSECTION a_hSection, TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType)
{
    TSection *pSection = SECTION_GetSection(a_hSection);

    if (pSection)
        return pSection->GetFirstItem(a_pFilter, a_pszBuffer, a_pdwType);

    return NULL;
}

//****************************************************************************************
BOOL WINAPI TBASE_GetNextItem(HTBSECTION a_hSection, LPSTR a_pszBuffer, DWORD *a_pdwType)
{
    TSection *pSection = SECTION_GetSection(a_hSection);

    if (pSection)
        return pSection->GetNextItem(a_pszBuffer, a_pdwType);

    return NULL;
}

//****************************************************************************************
BOOL WINAPI TBASE_GetValue(HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE **a_ppValue)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->GetValue(a_pszName, a_ppValue);

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_SetValue(HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE *a_pValue)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->SetValue(a_pszName, a_pValue);

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_DeleteValue(HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwHint)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->DeleteValue(a_pszName, a_dwHint);

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_Edit(HTBSECTION a_hSection)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->Edit();

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_Update(HTBSECTION a_hSection)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->Update();

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_CancelUpdate(HTBSECTION a_hSection)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->CancelUpdate();

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_SectionExists(HTBSECTION a_hBase, LPCSTR a_pszPath, BOOL *a_pbExist)
{
    TCallParams inParams(SVF_SECTION_EXISTS);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return FALSE;
    //-----------------------------------------
    if (IsBadStringPtr(a_pszPath, 512) || IsBadWritePtr(a_pbExist, sizeof(BOOL)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    inParams.SetHandle      (SVP_HBASE, (HTBHANDLE)a_hBase);
    inParams.SetTextPointer (SVP_PATH,  a_pszPath); 

    bRetVal = pTask->CallServer(inParams, outParams);
    //-----------------------------------------
    if (bRetVal)
    {
        *a_pbExist = (BOOL)outParams.GetLong(SVP_EXIST);
    }
    return bRetVal;
}

//****************************************************************************************
BOOL WINAPI TBASE_ValueExists(HTBSECTION a_hParent, LPCSTR a_pszValue, BOOL *a_pbExist)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hParent);
    if (pSection)
        return pSection->ValueExists(a_pszValue, a_pbExist);

    return FALSE;
}

//****************************************************************************************
BOOL WINAPI TBASE_GetLastErrorInfo(TBASE_ERROR_INFO *a_pErrorInfo)
{
    TTaskInfo  *pTask = TASK_GetCurrent();

    //--------------------------------------------------------
    if (IsBadWritePtr(a_pErrorInfo, sizeof(TBASE_ERROR_INFO)))
    {
        return FALSE;
    }
    //--------------------------------------------------------
    memset(a_pErrorInfo, 0,  sizeof(TBASE_ERROR_INFO));
    if (pTask==NULL)
    {
        if(G_hGlobalMutex == NULL)
        {
            a_pErrorInfo->dwErrorCode = TRERR_SERVICE_UNAVAILABLE;
        }
        else
            a_pErrorInfo->dwErrorCode = TRERR_BUS_ERROR;
        return TRUE;
    }
    //--------------------------------------------------------
    pTask->FillErrorInfo(a_pErrorInfo);

    return TRUE;
}

//***************************************************
BOOL WINAPI TBASE_Flush()
{
    TCallParams  inParams(SVF_FLUSH);
    TCallParams  outParams;
    TTaskInfo   *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return FALSE;
    //-----------------------------------------
    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
BOOL WINAPI TBASE_WriteLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_GETDATA_CALLBACK a_pfnCallback, void* a_pvCallbackParam)
{
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hSection))
        return NULL;

    //-----------------------------------------
    return BLOB_WriteLongBinary(a_hSection, a_pszName, a_pfnCallback, a_pvCallbackParam);
}

//******************************************************************************
BOOL WINAPI TBASE_ReadLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_PUTDATA_CALLBACK a_pfnCallback, LPVOID a_pvCallbackParam)
{
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    return BLOB_ReadLongBinary(a_hSection, a_pszName, a_pfnCallback, a_pvCallbackParam);
}

//******************************************************************************
HTBSTREAMIN WINAPI TBASE_OpenLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName)
{
    if (IsBadStringPtr(a_pszName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    return BLOB_OpenLongBinary(a_hSection, a_pszName);
}

//******************************************************************************
BOOL WINAPI TBASE_RenameLongBinary(HTBSECTION a_hSection, LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    //-----------------------------------------
    if (IsBadStringPtr(a_pszOldName, 128) || IsBadStringPtr(a_pszNewName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hSection))
        return FALSE;

    return BLOB_RenameLongBinary(a_hSection, a_pszOldName, a_pszNewName);
}

//******************************************************************************
BOOL WINAPI TBASE_LinkLongBinary(HTBSECTION a_hSource, LPCSTR a_pszName, HTBSECTION a_hTarget, LPCSTR a_pszLinkName)
{
    //-----------------------------------------
    if (IsBadStringPtr(a_pszName, 128) || IsBadStringPtr(a_pszLinkName, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    if (!TBASE_Update(a_hTarget))
        return FALSE;

    return BLOB_LinkLongBinary(a_hSource, a_pszName, a_hTarget, a_pszLinkName);
}

//******************************************************************************
BOOL WINAPI TBASE_GetSectionName(HTBSECTION a_hSection, BOOL a_bFull, LPSTR a_pszBuffer)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->GetName(a_bFull, a_pszBuffer);

    return FALSE;
}

//******************************************************************************
BOOL WINAPI TBASE_GetSectionAccess(HTBSECTION a_hSection, DWORD *a_pdwAccess)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->GetAccess(a_pdwAccess);

    return FALSE;
}

//******************************************************************************
BOOL WINAPI TBASE_GetSectionAttributes(HTBSECTION a_hSection, DWORD *a_pdwAttrs)
{
    TSection *pSection = NULL;

    pSection = SECTION_GetSection(a_hSection);
    if (pSection)
        return pSection->GetAttributes(a_pdwAttrs);

    return FALSE;
}

//******************************************************************************
BOOL WINAPI TBASE_GetSectionLabel(HTBSECTION a_hSection, LPSTR a_pszLabel)
{
    TCallParams inParams(SVF_GET_SECTION_LABEL);
    TCallParams outParams;
    TTaskInfo  *pTask    = TASK_GetCurrent();
    LPCSTR      pszLabel = NULL;
    BOOL        bRetVal  = FALSE; 

    if (pTask==NULL)
        return FALSE;
    //-----------------------------------------
    if (IsBadWritePtr(a_pszLabel, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)a_hSection);

    bRetVal = pTask->CallServer(inParams, outParams);
    //-----------------------------------------
    if (bRetVal)
    {
        pszLabel   =       outParams.GetText(SVP_LABEL);
        strcpy(a_pszLabel, pszLabel);
        return TRUE;
    }
    return FALSE;
}

//******************************************************************************
BOOL WINAPI TBASE_SetSectionLabel(HTBSECTION a_hSection, LPCSTR a_pszLabel)
{
    TCallParams inParams(SVF_SET_SECTION_LABEL);
    TCallParams outParams;
    TTaskInfo  *pTask    = TASK_GetCurrent();
    LPCSTR      pszLabel = NULL;

    if (pTask==NULL)
        return FALSE;
    //-----------------------------------------
    if (IsBadStringPtr(a_pszLabel, 128))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    inParams.SetHandle     (SVP_HSECTION, (HTBHANDLE)a_hSection);
    inParams.SetTextPointer(SVP_LABEL,    a_pszLabel);  

    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
BOOL WINAPI TBASE_DeleteSectionLabel(HTBSECTION a_hSection)
{
    TCallParams inParams(SVF_DELETE_SECTION_LABEL);
    TCallParams outParams;
    TTaskInfo  *pTask    = TASK_GetCurrent();
    LPCSTR      pszLabel = NULL;

    if (pTask==NULL)
        return FALSE;
    //-----------------------------------------
    inParams.SetHandle     (SVP_HSECTION, (HTBHANDLE)a_hSection);

    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
BOOL WINAPI TBASE_GetSectionLinkCount(HTBSECTION a_hSection, DWORD *a_pdwLinkCount)
{
    TCallParams inParams(SVF_GET_SECTION_LINK_COUNT);
    TCallParams outParams;
    TTaskInfo  *pTask    = TASK_GetCurrent();

    if (pTask == NULL)
        return FALSE;
    //-----------------------------------------
    if (IsBadWritePtr(a_pdwLinkCount, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    //-----------------------------------------
    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)a_hSection);

    if (pTask->CallServer(inParams, outParams))
    {
        *a_pdwLinkCount = outParams.GetLong(SVP_LINK_COUNT);
        return TRUE;
    };
    return FALSE;
}

//**************************************************************************************
DWORD WINAPI TBASE_GetData(HTBSTREAMIN a_hStream, void* a_pvBuff, DWORD a_nCount)
{
    if(IsBadWritePtr(a_pvBuff, a_nCount))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return 0;
    }
    //-----------------------------------------
    TCallParams inParams(SVF_GET_DATA);
    TCallParams outParams;
    TTaskInfo  *pTask = TASK_GetCurrent();

    if (pTask == NULL)
        return FALSE;
    //-----------------------------------------
    inParams.SetHandle(SVP_HSTREAM, (HTBHANDLE)a_hStream);
    inParams.SetLong  (SVP_COUNT,   a_nCount);

    if (!pTask->CallServer(inParams, outParams))
        return 0;

    void *pvBuff = NULL;
    long cbCount = 0;
    if(!outParams.GetBuffer(SVP_BUFFER, &pvBuff, cbCount))
        return 0;

    memcpy(a_pvBuff, pvBuff, cbCount);
    return cbCount;
}

//**************************************************************************************
DWORD WINAPI TBASE_PutData(HTBSTREAMOUT a_hStream, void* a_pvBuff, DWORD a_nCount)
{
    if(IsBadReadPtr(a_pvBuff, a_nCount))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return 0;
    }
    //-----------------------------------------
    TCallParams inParams(SVF_PUT_DATA);
    TCallParams outParams;
    TTaskInfo  *pTask = TASK_GetCurrent();

    if (pTask == NULL)
        return FALSE;
    //-----------------------------------------
    inParams.SetHandle       (SVP_HSTREAM, (HTBHANDLE)a_hStream);
    inParams.SetBufferPointer(SVP_BUFFER, a_pvBuff, a_nCount);

    if (!pTask->CallServer(inParams, outParams))
        return 0;

    return outParams.GetLong(SVP_COUNT);
}

//**************************************************************************************
BOOL WINAPI TBASE_CloseInputStream(HTBSTREAMOUT a_hStream)
{
    return BLOB_CloseStream((HTBHANDLE)a_hStream, SVF_CLOSE_STREAMIN);
}

//**************************************************************************************
BOOL WINAPI TBASE_CloseOutputStream(HTBSTREAMOUT a_hStream)
{
    return BLOB_CloseStream((HTBHANDLE)a_hStream, SVF_CLOSE_STREAMOUT);
}

//**************************************************************************************
HTBACL WINAPI TBASE_AclCreate()
{
    return (HTBACL)(new TAccessControlList());
};

//**************************************************************************************
BOOL WINAPI TBASE_AclDestroy(HTBACL a_hAcl)
{
    ACL_IMPLEMENT_API(FALSE, a_hAcl, Destroy());
}

//**************************************************************************************
BOOL WINAPI TBASE_AclResetAccessRights(HTBACL a_hAcl, PSID a_sid)
{
    ACL_IMPLEMENT_API(FALSE, a_hAcl, ResetAccessRights(a_sid));
}

//**************************************************************************************
BOOL WINAPI TBASE_AclSetAccessRights(HTBACL a_hAcl, PSID a_sid, DWORD a_dwAccessRights)
{
    ACL_IMPLEMENT_API(FALSE, a_hAcl, SetAccessRights(a_sid, a_dwAccessRights));
}

//**************************************************************************************
BOOL WINAPI TBASE_AclGetAccessRights (HTBACL a_hAcl, PSID a_sid, DWORD *a_pdwAccessRights)
{
    ACL_IMPLEMENT_API(FALSE, a_hAcl, GetAccessRights(a_sid, a_pdwAccessRights));
}

//**************************************************************************************
int WINAPI TBASE_AclGetCount(HTBACL a_hAcl)
{
    ACL_IMPLEMENT_API(-1, a_hAcl, GetCount());
}

//**************************************************************************************
BOOL WINAPI TBASE_AclGetEntry(HTBACL a_hAcl, DWORD a_dwIndex, PSID *a_psid, DWORD *a_pdwAccessRights)
{
    ACL_IMPLEMENT_API(FALSE, a_hAcl, GetEntry(a_dwIndex, a_psid, a_pdwAccessRights));
}

//**************************************************************************************
BOOL WINAPI TBASE_GetSectionSecurity(
    HTBSECTION a_hParent,
    LPCSTR a_pszName,
    DWORD a_dwSecurityInformationFlags,
    PSID* a_ppOwnerSid,
    GUID* a_pProtectionDomain, 
    TBSECURITY_ATTRIBUTES *a_pSecAttrs)
{
    BOOL bParamsOK = TRUE;

    if(a_dwSecurityInformationFlags & TBSECURITY_INFORMATION::eOWNER)
    {
        bParamsOK = !IsBadWritePtr(a_ppOwnerSid, sizeof(PSID));
    }
    if(a_dwSecurityInformationFlags & TBSECURITY_INFORMATION::ePROTECTION_DOMAIN)
    {
        bParamsOK = !IsBadWritePtr(a_pProtectionDomain, sizeof(GUID));
    }
    if(a_dwSecurityInformationFlags & TBSECURITY_INFORMATION::eATTRIBUTES)
    {
        bParamsOK = !IsBadWritePtr(a_pSecAttrs, sizeof(TBSECURITY_ATTRIBUTES));
    }
    //-----------------------------------------
    if(!bParamsOK)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return 0;
    }
    //-----------------------------------------
    TTaskInfo *pTask = TASK_GetCurrent();
    if(!pTask)
        return FALSE;

    //-----------------------------------------
    TCallParams inParams(SVF_GET_SECTION_SECURITY);
    TCallParams outParams;

    inParams.SetHandle      (SVP_HPARENT,         (HTBHANDLE)a_hParent);
    inParams.SetTextPointer (SVP_NAME,                       a_pszName);
    inParams.SetLong        (SVP_SECURITY_INFORMATION_FLAGS, a_dwSecurityInformationFlags);

    //-----------------------------------------
    if(!pTask->CallServer(inParams, outParams))
        return FALSE;

    //-----------------------------------------
    void *pvBuff = NULL;
    long cbSize = 0;

    if(outParams.GetBuffer(SVP_OWNER, &pvBuff, cbSize))
    {
        std::vector<BYTE> &ownerSid = s_lastOwnerSid;

        if(ownerSid.size() == 0)
            ownerSid.resize(SECURITY_MAX_SID_SIZE);

        memcpy(&ownerSid[0], pvBuff, cbSize);
        *a_ppOwnerSid = &ownerSid[0];
    }
    //-----------------------------------------
    if(outParams.GetBuffer(SVP_PROTECTION_DOMAIN, &pvBuff, cbSize))
        memcpy(a_pProtectionDomain, pvBuff, sizeof(GUID));

    //-----------------------------------------
    UTIL_DeserializeSecurity(a_pSecAttrs, outParams);

    return TRUE;
}

//**************************************************************************************
BOOL WINAPI TBASE_SetSectionSecurity(
    HTBSECTION a_hParent, 
    LPCSTR a_pszName,
    TBSECURITY_TARGET::Enum a_target,
    TBSECURITY_OPERATION::Enum a_operation,
    TBSECURITY_ATTRIBUTES *a_pSecAttrs)
{
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    //-----------------------------------------
    if (pTask==NULL)
        return NULL;

    //-----------------------------------------
    if (IsBadStringPtr(a_pszName, 128) || IsBadReadPtr(a_pSecAttrs, sizeof(TBSECURITY_ATTRIBUTES)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return NULL;
    }

    //-----------------------------------------
    TCallParams inParams(SVF_SET_SECTION_SECURITY);
    TCallParams outParams;

    inParams.SetHandle      (SVP_HPARENT,       (HTBHANDLE)a_hParent);
    inParams.SetTextPointer (SVP_NAME,                     a_pszName);
    inParams.SetLong        (SVP_SECURITY_TARGET,    (long)a_target);
    inParams.SetLong        (SVP_SECURITY_OPERATION, (long)a_operation);

    if(!UTIL_SerializeSecurity(a_pSecAttrs, inParams))
        return FALSE;

    return pTask->CallServer(inParams, outParams);
}
