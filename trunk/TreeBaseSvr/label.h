#if !defined(_LABEL_H_)
#define _LABEL_H_

#include "section.h"

class TLabelSection: public TSharedSection
{
    LPVOID m_pvCache;
public:
    TLabelSection(long a_ndbId = 0);
    ~TLabelSection();
    virtual BOOL CanModifyLabel();
public:
	BOOL CreateLabel(long a_nSegId, LPCSTR a_pszLabel);
    BOOL ModifyLabel(LPCSTR a_pszOldLabel, LPCSTR a_pszNewLabel);
    BOOL DeleteLabel(LPCSTR a_pszLabel);
    BOOL EnsureValueByName(LPCSTR a_pszName);

    TBITEM_ENTRY* GetCacheLabelData(ID_ENTRY *a_pLabelId);
    TBITEM_ENTRY* GetCacheLabelDataByName(LPCSTR a_pszName);
    TBITEM_ENTRY* GetLabelData(ID_ENTRY *a_pLabelId);
};

class TLabelManagerBase
{
public:
    virtual void Delete()=0;
};

TLabelManagerBase *LABEL_CreateManager();
BOOL LABEL_GetSectionLabel(TSection *a_pSection, CString &a_strLabel);
BOOL LABEL_SetSectionLabel(TSection *a_pSection, LPCSTR a_pszLabel);
BOOL LABEL_DeleteSectionLabel(TSection *a_pSection);
BOOL LABEL_DeleteSectionSegmentLabel(long a_ndbId, long a_nSegSection);
BOOL LABEL_EnsureLabels(long a_ndbID);


#endif //_LABEL_H_