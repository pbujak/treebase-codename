// blob.h
#if !defined(_BLOB_H)
#define _BLOB_H

#include "TreeBaseInt.h"
#include "TreeBase.h"
#include <map>

BOOL  BLOB_SetValue(HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE *a_pValue);
BOOL  BLOB_GetValue(HTBSECTION a_hSection, LPCSTR a_pszName, TBASE_VALUE *a_pValue);
BOOL  BLOB_LinkLongBinary(HTBSECTION a_hSource, LPCSTR a_pszName, HTBSECTION a_hTarget, LPCSTR a_pszLinkName);
BOOL  BLOB_RenameLongBinary(HTBSECTION a_hSection, LPCSTR a_pszOldName, LPCSTR a_pszNewName);
HTBSTREAMOUT BLOB_CreateLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName);
HTBSTREAMIN  BLOB_OpenLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName);
BOOL  BLOB_CloseStream(HTBHANDLE a_hStream, int a_nFunction);
BOOL BLOB_ReadLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_PUTDATA_CALLBACK a_pfnCallback, void* a_pvCallbackParam);
BOOL BLOB_WriteLongBinary(HTBSECTION a_hSection, LPCSTR a_pszName, BLOB_GETDATA_CALLBACK a_pfnCallback, void* a_pvCallbackParam);

class TStream
{
};

class TStreamContainer
{
public:
    std::map<HTBSTREAMIN, TStream> m_setStreamIn;
    std::map<HTBSTREAMOUT, TStream> m_setStreamOut;
public:
    TStreamContainer();
    ~TStreamContainer();
};

#endif //_BLOB_H
