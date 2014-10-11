// TBaseObj.h: interface for the CTBValue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TBASEOBJ_H__5256E276_E9A4_496B_B5DE_8F1BAFEE202C__INCLUDED_)
#define AFX_TBASEOBJ_H__5256E276_E9A4_496B_B5DE_8F1BAFEE202C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "treebase.h"
#include <atlsecurity.h>

class CTBException: public CException
{
public:
    TBASE_ERROR_INFO m_errInfo;
public:
	void Throw();
    CTBException();
    virtual int ReportError( UINT nType = MB_OK, UINT nMessageID = 0 );
};

class CTBValue  
{
public:
    TBASE_VALUE *m_pValue;
public:
	void Detach();
	void DeleteValue(LPCSTR a_pszName, DWORD a_nDelHint);
	void GetName(BOOL a_bFull, CString &a_strName);
	CTBValue();
	virtual ~CTBValue();

    operator TBASE_VALUE*()
    {
        return m_pValue;
    }
    TBASE_VALUE* operator->()
    {
        return m_pValue;
    }

    CTBValue& operator=(long a_nInteger);
    CTBValue& operator=(LPCSTR a_pszText);
    CTBValue& operator=(float a_dFloat);
    CTBValue& operator=(CRect a_rc);
    CTBValue& operator=(CPoint a_point);
    CTBValue& operator=(FLOAT_RECT  &a_fRect);
    CTBValue& operator=(FLOAT_POINT &a_fPoint);

    CTBValue(long a_nInteger)
    {
        m_pValue = NULL;
        operator=(a_nInteger);
    }
    CTBValue(LPCSTR a_pszText)
    {
        m_pValue = NULL;
        operator=(a_pszText);
    }
};

class CTBSecurityAttributes;

class CTBSection
{
public:
    HTBSECTION m_hSection;
public:
	BOOL Export(CFile *a_pFile);
	BOOL Import(CFile *a_pFile);
	DWORD GetAccess();
	DWORD GetAttributes();
	DWORD GetLinkCount();
	void CreateSectionLink(CTBSection *a_pSourceBase, LPCSTR a_pszSourcePath, LPCSTR a_pszTargetName);
	void RenameSection(LPCSTR a_pszOldName, LPCSTR a_pszNewName);
	BOOL SectionExists(LPCSTR a_pszName);

    void GetSectionSecurity(
        LPCSTR a_pszName,
        DWORD a_dwSecurityInformationFlags,
        ATL::CSid *a_pOwner,
        GUID *a_pProtectionDomain, 
        CTBSecurityAttributes *a_pSecAttrs);

    void SetSectionSecurity(
        LPCSTR a_pszName,
        TBSECURITY_TARGET::Enum a_target,
        TBSECURITY_OPERATION::Enum a_operation,
        CTBSecurityAttributes *a_pSecAttrs);

    void Create(CTBSection *a_pBase, LPCSTR a_pszName, DWORD a_dwAttrs, CTBSecurityAttributes *a_pSecAttrs = NULL);
	void Open(CTBSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode = TBOPEN_MODE_DEFAULT);
    void DeleteSection(LPCSTR a_pszName);
	BOOL GetFirstItem(CString &a_strName, DWORD &a_rdwType);
	BOOL GetNextItem (CString &a_strName, DWORD &a_rdwType);
    void GetName(BOOL a_bFull, CString &a_strName);
    void DeleteValue(LPCSTR a_pszName, DWORD a_dwDelHint);

    void GetLabel(CString &a_strLabel);
    void SetLabel(LPCSTR   a_pszLabel);
    void DeleteLabel();

    void Close();

    CTBSection();
    CTBSection(HTBSECTION a_hSection);
    ~CTBSection();
    
    HTBSECTION GetSafeHandle();
    void GetValue(LPCSTR a_pszName, CTBValue *a_pValue);
    void SetValue(LPCSTR a_pszName, CTBValue *a_pValue);
    void SetLongBinary(LPCSTR a_pszName, CFile *a_pFile);
    void GetLongBinary(LPCSTR a_pszName, CFile *a_pFile);
    void Edit();
    void Update();
    void CancelUpdate();

    static CTBSection tbRootSection;
};

class CTBAcl
{
public:
    HTBACL m_hAcl;
public:
    inline CTBAcl()
    {
        m_hAcl = TBASE_AclCreate();
    }
    inline ~CTBAcl()
    {
        TBASE_AclDestroy(m_hAcl);
    }

    inline operator HTBACL()
    {
        return m_hAcl;
    }
    void Reset(HTBACL a_hAcl = NULL);
    DWORD GetRights(const ATL::CSid &a_sid) const;
    void SetRights(const ATL::CSid &a_sid, DWORD a_dwRights);
    void ResetRights(const ATL::CSid &a_sid);
    void GetEntry(DWORD a_dwIndex, ATL::CSid &a_sid, DWORD & a_dwAccessRights) const;
    int GetCount() const;

};

class CTBSecurityAttributes
{
public:
    TBSECURITY_ATTRIBUTES m_securityAttributes;
    CTBAcl                m_blackList;
    CTBAcl                m_whiteList;
public:
    CTBSecurityAttributes(DWORD a_dwAccessRightsOwner = 0, DWORD a_dwAccessRightsAdmins = 0, DWORD a_dwAccessRightsAll = 0);
    inline operator TBSECURITY_ATTRIBUTES*()
    {
        return &m_securityAttributes;
    }
    inline TBSECURITY_ATTRIBUTES* operator->()
    {
        return &m_securityAttributes;
    }
};

inline void CloseStream(HTBSTREAMIN a_hStream)
{
    TBASE_CloseInputStream(a_hStream);
}

inline void CloseStream(HTBSTREAMOUT a_hStream)
{
    TBASE_CloseOutputStream(a_hStream);
}

template<typename _STREAM>
class TStream
{
    _STREAM m_hStream;
public:
    inline TStream(_STREAM a_hStream = NULL): m_hStream(a_hStream)
    {
    }

    inline ~TStream()
    {
        if(m_hStream)
            CloseStream(m_hStream);
    }

    inline operator _STREAM()
    {
        return m_hStream;
    }

    TStream const& operator=(_STREAM a_hStream)
    {
        close();
        m_hStream = a_hStream;
        return *this;
    }

    inline void close()
    {
        if(m_hStream)
            CloseStream(m_hStream);
        m_hStream = NULL;
    }
};


#endif // !defined(AFX_TBASEOBJ_H__5256E276_E9A4_496B_B5DE_8F1BAFEE202C__INCLUDED_)
