#include "stdafx.h"
#include "tbcore.h"

static LPCSTR G_pszHeader = "TreeBase Export";

enum
{
    eExpValue,
    eExpSection,
    eExpEndSection,
    eEndAll
};

class TExportStream
{
    TBASE_EXPORT_STREAM *m_pStream;
public:
    TExportStream(TBASE_EXPORT_STREAM *a_pStream=NULL): m_pStream(a_pStream) {};

    BOOL Read (DWORD a_dwSize, LPVOID a_pvBuff);
    BOOL Write(DWORD a_dwSize, LPVOID a_pvBuff);
    long ReadLong();
    BOOL WriteLong(long a_nValue);
    BOOL ReadString(TString &a_strValue);
    BOOL WriteString(LPCSTR a_pszValue);
};

//**************************************************************************************
BOOL TExportStream::Read(DWORD a_dwSize, LPVOID a_pvBuff)
{
    TBASE_EXPORT_STREAM_CALLBACK  pfnCallBack = NULL;

    if (m_pStream)
    {
        pfnCallBack = m_pStream->pfnCallBack;

        return (*pfnCallBack)(TBEXP_STREAM_READ, a_dwSize, a_pvBuff, m_pStream->pvCookie);
    }
    return FALSE;
}

//**************************************************************************************
BOOL TExportStream::Write(DWORD a_dwSize, LPVOID a_pvBuff)
{
    TBASE_EXPORT_STREAM_CALLBACK  pfnCallBack = NULL;

    if (m_pStream)
    {
        pfnCallBack = m_pStream->pfnCallBack;

        return (*pfnCallBack)(TBEXP_STREAM_WRITE, a_dwSize, a_pvBuff, m_pStream->pvCookie);
    }
    return FALSE;
}

//**************************************************************************************
long TExportStream::ReadLong()
{
    long nValue = 0;
    if (Read(sizeof(long), &nValue))
        return nValue;
    return -1;
}

//**************************************************************************************
inline BOOL TExportStream::WriteLong(long a_nValue)
{
    return Write(sizeof(long), &a_nValue);
}

//**************************************************************************************
BOOL TExportStream::ReadString(TString &a_strValue)
{
    long  nLen    = ReadLong();
    LPSTR pszText = NULL;
    BOOL  bRetVal = FALSE;  

    if (nLen>=0)
        bRetVal = TRUE;

    if (nLen>0)
    {
        bRetVal = FALSE;  
        pszText = (LPSTR)malloc(nLen+1);
        if(pszText)
        {
            memset(pszText, 0, nLen+1);
            bRetVal = Read(nLen, pszText);
            if (bRetVal)
            {
                a_strValue = pszText;
            }
            free(pszText);
        }
    }
    else
    {
        a_strValue.erase();
    }
    return bRetVal;
}

//**************************************************************************************
inline BOOL TExportStream::WriteString(LPCSTR a_pszValue)
{
    long nLen = strlen(a_pszValue);
    if (!WriteLong(nLen))
        return FALSE;
    return Write(nLen, (LPVOID)a_pszValue);
}

//**************************************************************************************
static BOOL ExportSection(HTBSECTION a_hSection, TExportStream &a_Stream)
{
/*
    char      szName [128] = {0};
    char      szLabel[128] = {0};
    DWORD     dwType       =  0;
    long      cbSize       =  0;
    BOOL      bFound       =  FALSE;
    HTBSECTION hSection     =  NULL;
    TBASE_VALUE *pValue    =  NULL;
    HGLOBAL      hGlobal   =  NULL;
    LPVOID       pvData    =  NULL;
    BOOL         bOK       =  FALSE; 
    DWORD        dwMask    =  0;
    
    //----------------------------------------------------------
    bFound = TBASE_GetFirstItem(a_hSection, NULL, szName, &dwType);
    while (bFound)
    {
        if(dwType==TBVTYPE_SECTION)
        {
            hSection = TBASE_OpenSection(a_hSection, szName);
            if (hSection==NULL)
                return FALSE;

            bOK =        TBASE_GetSectionLabel(hSection, szLabel);
            bOK = bOK && TBASE_GetSectionAccessMask(hSection, &dwMask);

            bOK = bOK && a_Stream.WriteLong(eExpSection);
            bOK = bOK && a_Stream.WriteString(szName);
            bOK = bOK && a_Stream.WriteString(szLabel);
            bOK = bOK && a_Stream.WriteLong((long)dwMask);
            bOK = bOK && ExportSection(hSection, a_Stream);
            bOK = bOK && a_Stream.WriteLong(eExpEndSection);

            TBASE_CloseSection(hSection);
        }
        else
        {
            if (!TBASE_GetValue(a_hSection, szName, &pValue))
                return FALSE;

            cbSize = pValue->cbSize;
            bOK =        a_Stream.WriteLong(eExpValue);
            bOK = bOK && a_Stream.WriteString(szName);
            bOK = bOK && a_Stream.WriteLong(cbSize);
            bOK = bOK && a_Stream.Write(cbSize, pValue);
            if (pValue->dwType==TBVTYPE_LONGBINARY)
            {
                hGlobal = pValue->data.blob.hData;
                cbSize  = pValue->data.blob.cbSize;
                pvData  = GlobalLock(hGlobal);
                if (pvData)
                {
                    bOK = bOK && a_Stream.WriteLong(cbSize);
                    bOK = bOK && a_Stream.Write(cbSize, pvData);
                    GlobalUnlock(hGlobal);
                }
            }
        }
        if (!bOK)
            break;
        bFound = TBASE_GetNextItem(a_hSection, szName, &dwType);
    };
    //----------------------------------------------------------
    if (pValue)
        TBASE_FreeValue(pValue);
    return bOK;
*/ return FALSE;
}

//**************************************************************************************
static BOOL ImportSection(HTBSECTION a_hSection, TExportStream &a_Stream)
{
/*
    long      cbSize = 0;  
    long      nCode  = 0;
    BOOL      bOK    = TRUE;
    BOOL      bExist = FALSE;
    BOOL      bLink  = FALSE;
    TString   strName, strLabel;
    DWORD     dwMask = 0;  
    char      szLabelPath[512] = {0};
    HTBSECTION hSection = NULL;
    TBASE_VALUE *pValue = NULL, *pPreValue = NULL;
    HGLOBAL   hData  = NULL;
    LPVOID    pvData = NULL;  

    //----------------------------------------------------------
    do
    {
        bLink = FALSE;
        if (pValue)
        {
            TBASE_FreeValue(pValue);
            pValue = NULL;
        }
        if (pPreValue)
        {
            free(pPreValue);
            pPreValue = NULL;
        }
        hSection = NULL;
        bLink    = FALSE;
        nCode = a_Stream.ReadLong();
        if (nCode==eExpEndSection || nCode==eEndAll)
        {
            if (a_hSection)
                TBASE_Update(a_hSection);
            break;
        }

        //--------------------------------------------------
        if (nCode==eExpSection)
        {
            bOK = bOK && a_Stream.ReadString(strName);
            bOK = bOK && a_Stream.ReadString(strLabel);
            if (bOK)
                dwMask = (DWORD)a_Stream.ReadLong();
            if (!bOK)
                break;
            if (a_hSection)
            {
                if (strLabel.length()>0)
                {
                    wsprintf(szLabelPath, "~\\%s", strLabel.c_str());
                    bLink = TBASE_SectionExists(NULL, szLabelPath, &bExist) && bExist;
                    if (bLink)
                    {
                       bOK = TBASE_CreateSectionLink(NULL, a_hSection, szLabelPath, strName.c_str());
                    };
                }
                if (!bLink)
                {
                    hSection = TBASE_CreateSection(a_hSection, strName.c_str(), dwMask);
                    bOK = (hSection!=NULL);
                    if (!bOK)
                        break;
                    if (strLabel.length()>0)
                        bOK = TBASE_SetSectionLabel(hSection, strLabel.c_str());                    
                }
            }
            bOK = bOK && ImportSection(hSection, a_Stream);
            if (hSection)
                TBASE_CloseSection(hSection);
        }
        //--------------------------------------------------
        if (nCode==eExpValue)
        {
            bOK = bOK && a_Stream.ReadString(strName);
            if (bOK)
            {
                cbSize = a_Stream.ReadLong();
                bOK = (cbSize>=0);
            }
            if (bOK)
            {
                bOK = FALSE;
                pPreValue = (TBASE_VALUE*)malloc(cbSize);
                if (pPreValue)
                {
                    bOK = a_Stream.Read(cbSize, pPreValue);
                    if (pPreValue->dwType==TBVTYPE_LONGBINARY)
                    {
                        bOK = FALSE;
                        cbSize = a_Stream.ReadLong();
                        hData  = GlobalAlloc(GMEM_MOVEABLE, cbSize);
                        if (hData)
                        {
                            pvData = GlobalLock(hData);
                            bOK = a_Stream.Read(cbSize, pvData);
                            GlobalUnlock(hData);
                            pPreValue->data.blob.hData = hData;
                        }
                    }
                    if (bOK)
                        pValue = TBASE_AllocValue(pPreValue->dwType, &pPreValue->data, pPreValue->cbSize);
                    bOK = (pValue!=NULL);
                    if (!pValue)
                    {
                        if (hData)
                            GlobalFree(hData);
                    }
                }
            }
            if (!bOK)
                break;
            if (a_hSection)
                TBASE_SetValue(a_hSection, strName.c_str(), pValue);
        }
    }
    while(1);
    //----------------------------------------------------------
    if (pValue)
        TBASE_FreeValue(pValue);
    if (pPreValue)
        free(pPreValue);
    return bOK;
    */
    return FALSE;
}

//**************************************************************************************
static BOOL _CheckParams(TBASE_EXPORT_STREAM *a_pStream)
{
    BOOL                          bOK         = FALSE;
    TBASE_EXPORT_STREAM_CALLBACK  pfnCallBack = NULL;

    bOK = (!IsBadReadPtr(a_pStream, sizeof(TBASE_EXPORT_STREAM)));
    if (bOK)
    {
        pfnCallBack = a_pStream->pfnCallBack;
        bOK = !IsBadCodePtr((FARPROC)pfnCallBack);
    }
    if (!bOK)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }
    return bOK;
}



//**************************************************************************************
BOOL WINAPI  TBASE_ExportSection(HTBSECTION a_hSection, TBASE_EXPORT_STREAM *a_pStream)
{
    TExportStream  stream(a_pStream);
    BOOL           bOK = FALSE;

    bOK = _CheckParams(a_pStream);

    bOK = bOK && stream.WriteString(G_pszHeader);
    bOK = bOK && ExportSection(a_hSection, stream);
    bOK = bOK && stream.WriteLong(eExpEndSection);

    return bOK;
}

//**************************************************************************************
BOOL WINAPI  TBASE_ImportSection(HTBSECTION a_hSection, TBASE_EXPORT_STREAM *a_pStream, BOOL a_bShareLabels)
{
    TExportStream  stream(a_pStream);
    BOOL           bOK = FALSE;
    TString        strHeader; 

    bOK = _CheckParams(a_pStream);

    bOK = bOK && stream.ReadString(strHeader);
    bOK = bOK && (strHeader==G_pszHeader);
    bOK = bOK && ImportSection(a_hSection, stream);

    return bOK;
}

