#include "stdafx.h"
#include "blob.h"
#include "tbcore.h"
#include "lpc.h"
#include "lpccodes.h"

namespace
{

//-----------------------------------------------------
template<class H>
class TScopedHandle
{
    H m_handle;
public:
    inline TScopedHandle(H a_handle = NULL): m_handle(a_handle) {};

    TScopedHandle& operator=(H a_handle)
    {
        if(m_handle)
            CloseHandleT(m_handle);
        
        m_handle = a_handle;
        return *this;
    }

    bool operator!()
    {
        return m_handle == NULL;
    }

    operator H()
    {
        return m_handle;
    }

    ~TScopedHandle()
    {
        if(m_handle)
            CloseHandleT(m_handle);
    }
};

//-----------------------------------------------------
inline void CloseHandleT(HTBSECTION a_hSection)
{
    TBASE_CloseSection(a_hSection);
}
//-----------------------------------------------------
inline void CloseHandleT(HTBSTREAMOUT a_hOut)
{
    BLOB_CloseStream((HTBHANDLE)a_hOut, SVF_CLOSE_STREAMOUT);
}
//-----------------------------------------------------
inline void CloseHandleT(HTBSTREAMIN a_hOut)
{
    BLOB_CloseStream((HTBHANDLE)a_hOut, SVF_CLOSE_STREAMIN);
}

//==============================================================================
class TWriteLongBinary: public TTaskState
{
public:
    TScopedHandle<HTBSECTION> m_hLB;
    TScopedHandle<HTBSTREAMOUT> m_hStream;
    void *m_pvBuff;
    BOOL  m_bLock;
    TSection *m_pSection;
public:
    TWriteLongBinary(TSection *a_pSection);
    ~TWriteLongBinary();
    void taskAttach();
    void taskDetach();
};

//==============================================================================
class TReadLongBinary: public TTaskState
{
public:
    TScopedHandle<HTBSTREAMIN> m_hStream;
    void *m_pvBuff;
public:
    TReadLongBinary();
    ~TReadLongBinary();
    void taskAttach();
    void taskDetach();
};

} // namespace

//***************************************************
HTBSECTION BLOB_CreateBlobTempSection(HTBSECTION a_hSection, LPCSTR a_pszName)
{
    TCallParams inParams(SVF_CREATE_BLOB_TEMP_SECTION);
    TCallParams outParams;
    BOOL        bRetVal = FALSE;
    TTaskInfo  *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return NULL;

    inParams.SetHandle      (SVP_HSECTION, (HTBHANDLE)a_hSection);
    inParams.SetTextPointer (SVP_NAME,     a_pszName);
    if (!pTask->CallServer(inParams, outParams))
        return NULL;

    return UTIL_GetSectionFromOutput(pTask, outParams);
}

//******************************************************************************
BOOL BLOB_CommitBlob(HTBSECTION a_hSection, LPCSTR a_pszName, LPCSTR a_pszTempSection)
{
    TCallParams   inParams(SVF_COMMIT_BLOB);
    TCallParams   outParams;

    TTaskInfo  *pTask = TASK_GetCurrent();

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION,     (HTBHANDLE)a_hSection);
    inParams.SetText  (SVP_NAME,         a_pszName);
    inParams.SetText  (SVP_TEMP_SECTION, a_pszTempSection);

    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
HTBSTREAMOUT BLOB_CreateLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName)
{
    TCallParams   inParams(SVF_CREATE_LONG_BINARY);
    TCallParams   outParams;

    TTaskInfo  *pTask = TASK_GetCurrent();

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)a_hSection);
    inParams.SetText  (SVP_NAME,     a_pszName);

    if(!pTask->CallServer(inParams, outParams))
        return NULL;

    return (HTBSTREAMOUT)outParams.GetHandle(SVP_HSTREAM);
}

//******************************************************************************
HTBSTREAMIN BLOB_OpenLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName)
{
    TCallParams   inParams(SVF_OPEN_LONG_BINARY);
    TCallParams   outParams;

    TTaskInfo  *pTask = TASK_GetCurrent();

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle(SVP_HSECTION, (HTBHANDLE)a_hSection);
    inParams.SetText  (SVP_NAME,     a_pszName);

    if(!pTask->CallServer(inParams, outParams))
        return NULL;

    return (HTBSTREAMIN)outParams.GetHandle(SVP_HSTREAM);
}

//******************************************************************************
BOOL BLOB_RenameLongBinary(HTBSECTION a_hSection, LPCSTR a_pszOldName, LPCSTR a_pszNewName)
{
    TCallParams   inParams(SVF_RENAME_LONG_BINARY);
    TCallParams   outParams;
    TTaskInfo    *pTask   = TASK_GetCurrent();

    if (pTask == NULL)
        return NULL;

    inParams.SetHandle      (SVP_HSECTION, (HTBHANDLE)a_hSection);
    inParams.SetTextPointer (SVP_OLDNAME,  a_pszOldName);
    inParams.SetTextPointer (SVP_NEWNAME,  a_pszNewName);

    return pTask->CallServer(inParams, outParams);
}


//******************************************************************************
BOOL  BLOB_LinkLongBinary(HTBSECTION a_hSource, LPCSTR a_pszName, HTBSECTION a_hTarget, LPCSTR a_pszLinkName)
{
    TCallParams   inParams(SVF_LINK_LONG_BINARY);
    TCallParams   outParams;
    TTaskInfo    *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return FALSE;

    inParams.SetHandle       (SVP_HSOURCE_SECTION, (HTBHANDLE)a_hSource);
    inParams.SetHandle       (SVP_HTARGET_SECTION, (HTBHANDLE)a_hTarget);
    inParams.SetTextPointer  (SVP_NAME,            a_pszName);
    inParams.SetTextPointer  (SVP_LINKNAME,        a_pszLinkName);

    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
BOOL  BLOB_CloseStream(HTBHANDLE a_hStream, int a_nFunction)
{
    TCallParams   inParams(a_nFunction);
    TCallParams   outParams;
    TTaskInfo    *pTask   = TASK_GetCurrent();

    if (pTask==NULL)
        return FALSE;

    inParams.SetHandle(SVP_HSTREAM, a_hStream);

    return pTask->CallServer(inParams, outParams);
}

//******************************************************************************
BOOL BLOB_ReadLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_PUTDATA_CALLBACK a_pfnCallback, void* a_pvCallbackParam)
{
    TSection *pSection = SECTION_GetSection(a_hSection);
    //-----------------------------------------------------------------------
    if(pSection == NULL)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, a_pszName, pSection->m_strName.c_str());
        return FALSE;
    }
    //-----------------------------------------------------------------------
    std::auto_ptr<TReadLongBinary> data(new TReadLongBinary());
    //-----------------------------------------------------------------------
    {
        data->m_hStream = BLOB_OpenLongBinary(a_hSection, a_pszName);
        if(!data->m_hStream)
            return FALSE;

        DWORD dwSize = 0x4000;
        data->m_pvBuff = malloc(dwSize);

        do
        {
            dwSize = TBASE_GetData(data->m_hStream, data->m_pvBuff, dwSize);

            if(dwSize == 0)
                break;

            data->taskAttach();
            BOOL bOK = (*a_pfnCallback)(a_pvCallbackParam, dwSize, data->m_pvBuff);
            data->taskDetach();

            if(!bOK) break;
        }while(dwSize >= 0x4000);

        free(data->m_pvBuff);
        data->m_pvBuff = NULL;
        data->m_hStream = NULL;
    }
    //-----------------------------------------------------------------------
    return TRUE;
}

//******************************************************************************
BOOL BLOB_WriteLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_GETDATA_CALLBACK a_pfnCallback, void* a_pvCallbackParam)
{
    TSection *pSection = SECTION_GetSection(a_hSection);
    //-----------------------------------------------------------------------
    if(pSection == NULL)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, a_pszName, pSection->m_strName.c_str());
        return FALSE;
    }
    //-----------------------------------------------------------------------
    if(pSection->m_bReadOnly)
    {
        TASK_SetErrorInfo(TRERR_ACCESS_DENIED, a_pszName, pSection->m_strName.c_str());
        return FALSE;
    }
    if(pSection->m_dwAttrs & TBATTR_NOVALUES)
    {
        TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName, pSection->m_strName.c_str());
        return FALSE;
    }
    //-----------------------------------------------------------------------
    { // exists element 
        char buff[MAX_PATH] = {0};
        DWORD dwType = 0;
        TBASE_ITEM_FILTER filter = {0}; 
        strcpy(filter.szPattern, a_pszName);

        if(TBASE_GetFirstItem(a_hSection, &filter, buff, &dwType))
        {
            if(dwType != TBVTYPE_LONGBINARY)
            {
                TASK_SetErrorInfo(TRERR_ILLEGAL_OPERATION, a_pszName, pSection->m_strName.c_str());
                return FALSE;
            }
        }
    }
    //-----------------------------------------------------------------------
    std::auto_ptr<TWriteLongBinary> data(new TWriteLongBinary(pSection));
    if(!data->m_bLock)
        return FALSE;
    //-----------------------------------------------------------------------
    char szLBSection[MAX_PATH] = {0};   
    sprintf(szLBSection, "LB_%X_%s", a_hSection, a_pszName);
    //-----------------------------------------------------------------------
    {
        data->m_hLB = BLOB_CreateBlobTempSection(a_hSection, szLBSection);
        if(!data->m_hLB)
            return FALSE;

        data->m_hStream = BLOB_CreateLongBinary(data->m_hLB, "LB");
        if(!data->m_hStream)
            return NULL;

        DWORD dwSize = 0x4000;
        data->m_pvBuff = malloc(dwSize);

        do
        {
            data->taskAttach();
            BOOL bOK = (*a_pfnCallback)(a_pvCallbackParam, &dwSize, data->m_pvBuff);
            data->taskDetach();

            if(!bOK) break;
            if(!TBASE_PutData(data->m_hStream, data->m_pvBuff, dwSize) || dwSize < 0x4000)
                break;
        }while(true);

        free(data->m_pvBuff);
        data->m_pvBuff = NULL;
        data->m_hStream = NULL;
        data->m_hLB = NULL;
    }
    //-----------------------------------------------------------------------
    return BLOB_CommitBlob(a_hSection, a_pszName, szLBSection);
}

//******************************************************************************
TReadLongBinary::TReadLongBinary(): m_pvBuff(NULL)
{
}

//******************************************************************************
TReadLongBinary::~TReadLongBinary()
{
    if(m_pvBuff)
        free(m_pvBuff);

    m_hStream = NULL;

    taskDetach();
}

//******************************************************************************
void TReadLongBinary::taskAttach()
{
    TTaskInfo *pTask = TASK_GetCurrent(FALSE);
    if(pTask)
        pTask->m_pTaskState = this;
}

//******************************************************************************
void TReadLongBinary::taskDetach()
{
    TTaskInfo *pTask = TASK_GetCurrent(FALSE);
    if(pTask)
        pTask->m_pTaskState = NULL;
}

//******************************************************************************
TWriteLongBinary::TWriteLongBinary(TSection *a_pSection): m_pvBuff(NULL), m_pSection(a_pSection)
{
    m_bLock = m_pSection->LockSection(TRUE);
}

//******************************************************************************
TWriteLongBinary::~TWriteLongBinary()
{
    if(m_pvBuff)
        free(m_pvBuff);

    m_hStream = NULL;

    taskDetach();
    if(m_bLock)
        m_pSection->LockSection(FALSE);
}

//******************************************************************************
void TWriteLongBinary::taskAttach()
{
    TTaskInfo *pTask = TASK_GetCurrent(FALSE);
    if(pTask)
        pTask->m_pTaskState = this;
}

//******************************************************************************
void TWriteLongBinary::taskDetach()
{
    TTaskInfo *pTask = TASK_GetCurrent(FALSE);
    if(pTask)
        pTask->m_pTaskState = NULL;
}
