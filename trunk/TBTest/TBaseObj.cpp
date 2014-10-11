// TBaseObj.cpp: implementation of the CTBValue class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TBTest.h"
#include "TBaseObj.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

static CTBException  G_exception;

//*****************************************************************************************
static BOOL CALLBACK _GetDataCallback(LPVOID a_pvParam, DWORD *a_pdwSize, LPVOID a_pvBuff)
{
    CFile *pFile = (CFile *)a_pvParam;

    try
    {
        *a_pdwSize = pFile->Read(a_pvBuff, *a_pdwSize);
        return TRUE;
    }
    catch(CException *e)
    {
        e->Delete();
        return FALSE;
    }
}

//*****************************************************************************************
static BOOL CALLBACK _PutDataCallback(LPVOID a_pvParam, DWORD a_dwSize, LPVOID a_pvBuff)
{
    CFile *pFile = (CFile *)a_pvParam;

    try
    {
        pFile->Write(a_pvBuff, a_dwSize);
        return TRUE;
    }
    catch(CException *e)
    {
        e->Delete();
        return FALSE;
    }
}

CTBException::CTBException(): CException(FALSE)
{};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTBValue::CTBValue(): m_pValue(NULL)
{

}

CTBValue::~CTBValue()
{
    if (m_pValue!=NULL)
        TBASE_FreeValue(m_pValue);
}

//***********************************************************************
CTBValue& CTBValue::operator=(long a_nInteger)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }
    xData.int32 = a_nInteger;
    m_pValue = TBASE_AllocValue(TBVTYPE_INTEGER, &xData, -1);
    return *this;
};

//***********************************************************************
CTBValue& CTBValue::operator=(float a_dFloat)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }
    xData.float64 = a_dFloat;
    m_pValue = TBASE_AllocValue(TBVTYPE_FLOAT, &xData, -1);
    return *this;
};


//***********************************************************************
CTBValue& CTBValue::operator=(LPCSTR a_pszText)
{
    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }
    if (a_pszText)
    {
        m_pValue = TBASE_AllocValue(TBVTYPE_TEXT, (TBASE_VALUE_DATA*)a_pszText, -1);
    }
    return *this;
};

//***********************************************************************
CTBValue& CTBValue::operator=(CRect a_rc)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }

    xData.rect = a_rc;

    m_pValue = TBASE_AllocValue(TBVTYPE_RECT, &xData, -1);
    return *this;
};

//***********************************************************************
CTBValue& CTBValue::operator=(CPoint a_point)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }

    xData.point = a_point;

    m_pValue = TBASE_AllocValue(TBVTYPE_POINT, &xData, -1);
    return *this;
};

//***********************************************************************
CTBValue& CTBValue::operator=(FLOAT_RECT  &a_fRect)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }

    xData.frect = a_fRect;

    m_pValue = TBASE_AllocValue(TBVTYPE_POINT, &xData, -1);
    return *this;
};

//***********************************************************************
CTBValue& CTBValue::operator=(FLOAT_POINT &a_fPoint)
{
    TBASE_VALUE_DATA xData;

    if (m_pValue!=NULL)
    {
        TBASE_FreeValue(m_pValue);
        m_pValue = NULL;
    }

    xData.fpoint = a_fPoint;

    m_pValue = TBASE_AllocValue(TBVTYPE_POINT, &xData, -1);
    return *this;
};

//***********************************************************************
void CTBException::Throw()
{
    TBASE_GetLastErrorInfo(&m_errInfo);
    throw this;
}

//***********************************************************************
int CTBException::ReportError( UINT nType, UINT nMessageID)
{
    CString strMessage, strErrorName;

    switch (m_errInfo.dwErrorCode)
    {
        case TRERR_INVALID_NAME:
            strErrorName = "Invalid name";
            break;
        case TRERR_ITEM_NOT_FOUND:
            strErrorName = "Item not found";
            break;
        case TRERR_PATH_NOT_FOUND:
            strErrorName = "Path not found";
            break;
        case TRERR_ITEM_ALREADY_EXIST:
        case TRERR_ALREADY_EXIST:
            strErrorName = "Item already exists";
            break;
        case TRERR_ACCESS_DENIED:
            strErrorName = "Access denied";
            break;
        case TRERR_SHARING_DENIED:
            strErrorName = "Sharing denied";
            break;
        case TRERR_ILLEGAL_OPERATION:
            strErrorName = "Unsupported operation";
            break;
        case TRERR_CANNOT_CREATE_SECTION:
            strErrorName = "Cannot create section";
            break;
        case TRERR_SECTION_NOT_EMPTY:
            strErrorName = "Section not empty";
            break;
        case TRERR_INVALID_PARAMETER :
            strErrorName = "Invalid parameter";
            break;
        case TRERR_OUT_OF_DATA :
            strErrorName = "Out of data";
            break;
        case TRERR_DISK_FULL :
            strErrorName = "Disk full";
            break;
        case TRERR_CANNOT_CREATE_BLOB :
            strErrorName = "Cannot create BLOB";
            break;
        case TRERR_INVALID_TYPE :
            strErrorName = "Invalid type";
            break;
        case TRERR_INVALID_DATA :
            strErrorName = "Invalid data";
            break;
        case TRERR_BUS_ERROR :
            strErrorName = "Bus error";
            break;
        case TRERR_LABEL_ALREADY_USED:
            strErrorName = "Label already used";
            break;
        case TRERR_CANNOT_SET_LABEL:
            strErrorName = "Cannot set label";
            break;
        case TRERR_STORAGE_FAILURE:
            strErrorName = "Storage failure";
            break;
        case TRERR_SECTION_NOT_MOUNTED:
            strErrorName = "Section not mounted";
            break;
        case TRERR_SERVICE_UNAVAILABLE:
            strErrorName = "Service unavailable";
            break;
    }

    strMessage.Format("ERROR: %s\n Section: %s\n Item: %s", strErrorName, m_errInfo.szErrorSection, m_errInfo.szErrorItem);
    return AfxMessageBox(strMessage, nType, 0);
};

CTBSection CTBSection::tbRootSection(TBSECTION_ROOT);

//***********************************************************************
CTBSection::CTBSection(): m_hSection(NULL)
{
};

//***********************************************************************
CTBSection::CTBSection(HTBSECTION a_hSection): m_hSection(a_hSection)
{
};


//***********************************************************************
CTBSection::~CTBSection()
{
    Close();
};

//***********************************************************************
HTBSECTION CTBSection::GetSafeHandle()
{
    if (this!=NULL)
        return m_hSection;
    return NULL;
}

//***********************************************************************
void CTBSection::Open(CTBSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode)
{
    HTBSECTION hSection = NULL;

    if (m_hSection)
    {
        _ASSERT(FALSE);
        Close();
    };
    hSection = TBASE_OpenSection(a_pBase->GetSafeHandle(), a_pszPath, a_dwOpenMode);
    if (hSection==NULL)
        G_exception.Throw();

    m_hSection = hSection;
}

//***********************************************************************
void CTBSection::Create(CTBSection *a_pBase, LPCSTR a_pszName, DWORD a_dwAttrs, CTBSecurityAttributes *a_pSecAttrs)
{
    HTBSECTION hSection = NULL;

    if (m_hSection)
    {
        _ASSERT(FALSE);
        Close();
    };

    if(a_pSecAttrs)
    {
        if(a_pSecAttrs->m_blackList.GetCount() > 0)
            a_pSecAttrs->m_securityAttributes.hBlackList = a_pSecAttrs->m_blackList;
        if(a_pSecAttrs->m_whiteList.GetCount() > 0)
            a_pSecAttrs->m_securityAttributes.hWhiteList = a_pSecAttrs->m_whiteList;
    }

    hSection = TBASE_CreateSection(a_pBase->GetSafeHandle(), a_pszName, a_dwAttrs, (a_pSecAttrs ? &a_pSecAttrs->m_securityAttributes : NULL) );
    if (hSection==NULL)
        G_exception.Throw();

    m_hSection = hSection;
}

//***********************************************************************
void CTBSection::Close()
{
    if (m_hSection)
    {
        TBASE_CloseSection(m_hSection);
        m_hSection = NULL;
    }
}

//***********************************************************************
void CTBSection::Edit()
{
    if (!TBASE_Edit(m_hSection))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::Update()
{
    if (!TBASE_Update(m_hSection))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::CancelUpdate()
{
    if (!TBASE_CancelUpdate(m_hSection))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::DeleteSection(LPCSTR a_pszName)
{
    if (!a_pszName || !m_hSection)
    {
        _ASSERT(FALSE);
        return;
    }
    if (!TBASE_DeleteSection(m_hSection, a_pszName))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::GetValue(LPCSTR a_pszName, CTBValue *a_pValue)
{
    if (!a_pszName || !a_pValue)
    {
        _ASSERT(FALSE);
        return;
    }
    if (!TBASE_GetValue(m_hSection, a_pszName, &a_pValue->m_pValue))
    {
        a_pValue->m_pValue = NULL;
    };
}

//***********************************************************************
void CTBSection::SetValue(LPCSTR a_pszName, CTBValue *a_pValue)
{
    if (!a_pszName || !a_pValue)
    {
        _ASSERT(FALSE);
        return;
    }
    if (!TBASE_SetValue(m_hSection, a_pszName, a_pValue->m_pValue))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::SetLongBinary(LPCSTR a_pszName, CFile *a_pFile)
{
    a_pFile->Seek(0, CFile::begin);

    if(!TBASE_WriteLongBinary(m_hSection, a_pszName, _GetDataCallback, a_pFile))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::GetLongBinary(LPCSTR a_pszName, CFile *a_pFile)
{
    a_pFile->SetLength(0);

    if(!TBASE_ReadLongBinary(m_hSection, a_pszName, _PutDataCallback, a_pFile))
        G_exception.Throw();
}

//***********************************************************************
BOOL CTBSection::GetFirstItem(CString &a_strName, DWORD &a_rdwType)
{
    BOOL  bRes      = FALSE;
    LPSTR pszBuffer = a_strName.GetBuffer(128);

    if (!pszBuffer)
        return FALSE;
    bRes = TBASE_GetFirstItem(m_hSection, NULL, pszBuffer, &a_rdwType);
    a_strName.ReleaseBuffer();
    return bRes;
}

//***********************************************************************
BOOL CTBSection::GetNextItem(CString &a_strName, DWORD &a_rdwType)
{
    BOOL  bRes      = FALSE;
    LPSTR pszBuffer = a_strName.GetBuffer(128);

    if (!pszBuffer)
        return FALSE;
    bRes = TBASE_GetNextItem(m_hSection, pszBuffer, &a_rdwType);
    a_strName.ReleaseBuffer();
    return bRes;
}

//***********************************************************************
void CTBSection::GetName(BOOL a_bFull, CString &a_strName)
{
    LPSTR pszBuffer = a_strName.GetBuffer(256);

    if (!pszBuffer)
        return;
    TBASE_GetSectionName(m_hSection, a_bFull, pszBuffer);
    a_strName.ReleaseBuffer();
}

//***********************************************************************
void CTBSection::DeleteValue(LPCSTR a_pszName, DWORD a_dwDelHint)
{
    if (!TBASE_DeleteValue(m_hSection, a_pszName, a_dwDelHint))
        G_exception.Throw();
}

//***********************************************************************
void CTBValue::Detach()
{
    m_pValue = NULL;
}

//***********************************************************************
BOOL CTBSection::SectionExists(LPCSTR a_pszPath)
{
    BOOL bExists = FALSE;
    if (!TBASE_SectionExists(m_hSection, a_pszPath, &bExists))
        G_exception.Throw();
    return bExists;
}

//***********************************************************************
void CTBSection::RenameSection(LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    if (!TBASE_RenameSection(m_hSection, a_pszOldName, a_pszNewName))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::CreateSectionLink(CTBSection *a_pSourceBase, LPCSTR a_pszSourcePath, LPCSTR a_pszTargetName)
{
    if (!TBASE_CreateSectionLink(a_pSourceBase->GetSafeHandle(), m_hSection, a_pszSourcePath, a_pszTargetName))
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::GetLabel(CString &a_strLabel)
{
    LPSTR pszBuffer;
    BOOL  bRetVal;

    pszBuffer = a_strLabel.GetBuffer(128);
    bRetVal   = TBASE_GetSectionLabel(m_hSection, pszBuffer);
    a_strLabel.ReleaseBuffer();

    if (!bRetVal)
        G_exception.Throw();
}

//***********************************************************************
void CTBSection::SetLabel(LPCSTR a_pszLabel)
{
    if (!TBASE_SetSectionLabel(m_hSection, a_pszLabel))
        G_exception.Throw();
};

//***********************************************************************
void CTBSection::DeleteLabel()
{
    if (!TBASE_DeleteSectionLabel(m_hSection))
        G_exception.Throw();
}

//***********************************************************************
DWORD CTBSection::GetLinkCount()
{
    DWORD dwLinkCount = 0;
    if (!TBASE_GetSectionLinkCount(m_hSection, &dwLinkCount))
        G_exception.Throw();
    return dwLinkCount;
}

//***********************************************************************
DWORD CTBSection::GetAttributes()
{
    DWORD dwAttrs = 0;
    if (!TBASE_GetSectionAttributes(m_hSection, &dwAttrs))
        G_exception.Throw();
    return dwAttrs;
}

//***********************************************************************
DWORD CTBSection::GetAccess()
{
    DWORD dwAccess = 0;
    if (!TBASE_GetSectionAccess(m_hSection, &dwAccess))
        G_exception.Throw();
    return dwAccess;
}

//***********************************************************************
void CTBSection::GetSectionSecurity(
    LPCSTR a_pszName,
    DWORD a_dwSecurityInformationFlags,
    ATL::CSid *a_pOwner,
    GUID *a_pProtectionDomain, 
    CTBSecurityAttributes *a_pSecAttrs)
{
    TBSECURITY_ATTRIBUTES secAttrs = {0};
    
    PSID psidOwner = NULL;

    if(!TBASE_GetSectionSecurity(m_hSection, 
        a_pszName,
        a_dwSecurityInformationFlags,
        a_pOwner ? &psidOwner : NULL,
        a_pProtectionDomain,
        (a_pSecAttrs ? &secAttrs : NULL)))
    {
        G_exception.Throw();
    }

    if(a_pOwner)
        *a_pOwner = ATL::CSid(*(SID*)psidOwner);

    if(a_pSecAttrs)
    {
        a_pSecAttrs->m_securityAttributes = secAttrs;
        a_pSecAttrs->m_whiteList.Reset(secAttrs.hWhiteList);
        a_pSecAttrs->m_blackList.Reset(secAttrs.hBlackList);
    }
}

//***********************************************************************
void CTBSection::SetSectionSecurity(
    LPCSTR a_pszName,
    TBSECURITY_TARGET::Enum a_target,
    TBSECURITY_OPERATION::Enum a_operation,
    CTBSecurityAttributes *a_pSecAttrs)
{
    if(a_pSecAttrs)
    {
        if(a_pSecAttrs->m_blackList.GetCount() > 0)
            a_pSecAttrs->m_securityAttributes.hBlackList = a_pSecAttrs->m_blackList;
        if(a_pSecAttrs->m_whiteList.GetCount() > 0)
            a_pSecAttrs->m_securityAttributes.hWhiteList = a_pSecAttrs->m_whiteList;
    }

    if(!TBASE_SetSectionSecurity(m_hSection,
        a_pszName,
        a_target,
        a_operation,
        a_pSecAttrs ? &a_pSecAttrs->m_securityAttributes : NULL))
    {
        G_exception.Throw();
    }
}

//*****************************************************************
static BOOL CALLBACK ExportImportCB(UINT a_uCode, DWORD a_cbBytes, LPVOID a_pvBuff, LPVOID a_pvCookie)
{
    CFile *pFile = (CFile*)a_pvCookie;
    try{
        if (a_uCode==TBEXP_STREAM_READ)
        {
            pFile->Read(a_pvBuff, a_cbBytes);
        }
        else
        {
            pFile->Write(a_pvBuff, a_cbBytes);
        }
    }catch(CException *e)
    {
        return FALSE;
    }
    return TRUE;
};

//******************************************************************
BOOL CTBSection::Export(CFile *a_pFile)
{
    TBASE_EXPORT_STREAM stream = {0};

    a_pFile->SetLength(0);

    stream.pvCookie    = a_pFile;
    stream.pfnCallBack = ExportImportCB;

    return TBASE_ExportSection(m_hSection, &stream);
}

//******************************************************************
BOOL CTBSection::Import(CFile *a_pFile)
{
    TBASE_EXPORT_STREAM stream = {0};

    a_pFile->SeekToBegin();

    stream.pvCookie    = a_pFile;
    stream.pfnCallBack = ExportImportCB;

    return TBASE_ImportSection(m_hSection, &stream, 0);
}

//******************************************************************
void CTBAcl::SetRights(const ATL::CSid &a_sid, DWORD a_dwRights)
{
    if(!TBASE_AclSetAccessRights(m_hAcl, (PSID)a_sid.GetPSID(), a_dwRights))
        G_exception.Throw();
}

//******************************************************************
void CTBAcl::ResetRights(const ATL::CSid &a_sid)
{
    if(!TBASE_AclResetAccessRights(m_hAcl, (PSID)a_sid.GetPSID()))
        G_exception.Throw();
}

//******************************************************************
DWORD CTBAcl::GetRights(const ATL::CSid &a_sid) const
{
    DWORD dwRights = 0;

    if(!TBASE_AclGetAccessRights(m_hAcl, (PSID)a_sid.GetPSID(), &dwRights))
        G_exception.Throw();

    return dwRights;
}

//******************************************************************
int CTBAcl::GetCount() const
{
    int nCount = TBASE_AclGetCount(m_hAcl);

    if(nCount == -1)
        G_exception.Throw();

    return nCount;
}

//******************************************************************
void CTBAcl::GetEntry(DWORD a_dwIndex, ATL::CSid &a_sid, DWORD & a_dwAccessRights) const
{
    PSID psid = NULL;

    if(!TBASE_AclGetEntry(m_hAcl, a_dwIndex, &psid, &a_dwAccessRights))
        G_exception.Throw();

    a_sid = ATL::CSid((SID*)psid);
}

//******************************************************************
void CTBAcl::Reset(HTBACL a_hAcl)
{
    if(m_hAcl)
    {
        TBASE_AclDestroy(m_hAcl);
        m_hAcl = NULL;
    }
    m_hAcl = a_hAcl ? a_hAcl : TBASE_AclCreate();
}

//******************************************************************
CTBSecurityAttributes::CTBSecurityAttributes(DWORD a_dwAccessRightsOwner, DWORD a_dwAccessRightsAdmins, DWORD a_dwAccessRightsAll)
{
    m_securityAttributes.dwAccessRightsOwner = a_dwAccessRightsOwner;
    m_securityAttributes.dwAccessRightsAdmins = a_dwAccessRightsAdmins;
    m_securityAttributes.dwAccessRightsAll = a_dwAccessRightsAll;
    m_securityAttributes.hWhiteList = NULL;
    m_securityAttributes.hBlackList = NULL;
}