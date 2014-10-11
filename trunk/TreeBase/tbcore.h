// tbcore.h
#if !defined(_TBCORE_H_)
#define _TBCORE_H_

#include <string>
#include <map>
#include <set>
#include "treebaseint.h"
#include "lpc.h" 
#include "TValueMap.h"

#pragma warning(disable: 4786)

extern HANDLE G_hReady; 
extern HANDLE G_hGlobalMutex; 
extern HANDLE G_hModule;

class TCallParams;

class TCriticalSection
{
    CRITICAL_SECTION m_cs;
public:
    inline TCriticalSection()
    {
        InitializeCriticalSection(&m_cs);
    }
    inline ~TCriticalSection()
    {
        DeleteCriticalSection(&m_cs);
    }
    inline void Enter()
    {
        EnterCriticalSection(&m_cs);
    }
    inline void Leave()
    {
        LeaveCriticalSection(&m_cs);
    }
};

class TClientErrorInfo: public TErrorInfo
{
public:
    virtual void SetTaskErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection);
};

typedef std::basic_string<char> TStringBase;

class TString: public TStringBase
{
public:
    TString& operator=(LPCSTR a_pszText);
    TString& operator=(TStringBase &a_rText)
    {
        TStringBase::operator=(a_rText);
        return *this;
    };
};

class TDllMemAlloc: public TMemAlloc
{
public:
    virtual LPVOID Alloc(DWORD a_cbSize);
    virtual void   Free(LPVOID a_pvData);
    virtual LPVOID ReAlloc(LPVOID a_pvData, DWORD a_cbSize);
    virtual DWORD  MemSize(LPVOID a_pvData);
};

class TStreamContainer;

class TSection
{
protected:
    TSection(HTBSECTION a_hSection, LPCSTR a_pszName);
public:
    BOOL       m_bEditMode;
    HTBSECTION  m_hSection;
    LPVOID     m_pvNewValuesEnum;
    long       m_nNewValuesPos; 
    TValueMap  m_valCache;
    TValueMap  m_valForUpdate;
    TString    m_strName; 
    DWORD      m_dwAttrs;
    BOOL       m_bReadOnly, m_bCanGetValue;

    TItemFilterData      m_filter;
    ITERATED_ITEM_ENTRY *m_pIterBlock, *m_pIterCurrent;
    FETCH_VALUE_STRUCT  *m_pByID;

    static TSection* Create(HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwAttrs);
    ~TSection();
private:
    FETCH_VALUE_STRUCT  *FetchValueByID(DWORD a_dwHash, WORD a_wOrder);
    FETCH_VALUE_STRUCT  *FetchValueByName(LPCSTR a_pszName);
	ITERATED_ITEM_ENTRY *InternalGetFirstItems(TBASE_ITEM_FILTER *a_pFilter);
	ITERATED_ITEM_ENTRY *InternalGetNextItems();
    TBITEM_ENTRY        *FindInCacheById(DWORD a_dwHash, WORD a_wOrder);
    BOOL                 GetCacheValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue);
    BOOL                 DeleteLongBinary(LPCSTR a_pszName);
    BOOL                 FetchToCache(LPCSTR a_pszName);
    BOOL                 FlushUpdateValues();   

public:
    BOOL GetAccess(DWORD *a_pdwAccessMask);
    BOOL GetAttributes(DWORD *a_pdwAccessMask);
	BOOL GetName(BOOL a_bFull, LPSTR a_pszBuffer);
    void FreeByIDCache();
	BOOL DeleteValue(LPCSTR a_pszName, DWORD a_dwHint);
	BOOL Update();
	BOOL CancelUpdate();
	BOOL LockSection (BOOL a_bLock);
    BOOL GetNextItem (LPSTR a_pszBuffer, DWORD *a_pdwType);
    virtual BOOL GetFirstItem(TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType);

	virtual BOOL Edit();
	virtual BOOL GetValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue);
	virtual BOOL SetValue(LPCSTR a_pszName, TBASE_VALUE * a_pValue);

    BOOL ProcessBatch(LPVOID a_pvBatch);
    BOOL ValueExists (LPCSTR a_pszName, BOOL *a_pbExists);
};

class TNoValueSection: public TSection
{
public:
    inline TNoValueSection();
    inline TNoValueSection(HTBSECTION a_hSection, LPCSTR a_pszName): TSection(a_hSection, a_pszName){};

    virtual BOOL GetFirstItem(TBASE_ITEM_FILTER *a_pFilter, LPSTR a_pszBuffer, DWORD *a_pdwType);
	virtual BOOL Edit();
	virtual BOOL GetValue(LPCSTR a_pszName, TBASE_VALUE **a_ppValue);
};

typedef std::map<HTBSECTION, TSection*> TSectionMap;
typedef TSectionMap::iterator          TSectionMapIter; 

typedef std::set<LPVOID>   TPtrSet;
typedef TPtrSet::iterator  TPtrIter; 

//======================================================================
class TTaskState
{
public:
    virtual ~TTaskState() {};
};

//======================================================================
class TTaskInfo
{
    BOOL    m_bPipeClose;
    TPipe  *m_pPipe;
    HANDLE  m_hBlobBuf;
    TSectionMap   m_mapSections;
public:
    TPtrSet       m_setPointers;
    TTaskState   *m_pTaskState;
public:
    DWORD m_dwError;
    TString  m_strErrorItem;
    TString  m_strErrorSection;

    inline HANDLE GetBlobBufHFM(){return m_hBlobBuf; };
    TPipeFactory* GetPipeFactory();
public:
    void FillErrorInfo(TBASE_ERROR_INFO *a_pErrorInfo);
	void RemoveSection(HTBSECTION a_hSection);
	TSection* SectionFromHandle(HTBSECTION a_hSection);
	BOOL CallServer(TCallParams &a_inParams, TCallParams &a_outParams);
    TTaskInfo(): 
        m_hBlobBuf(NULL), m_bPipeClose(FALSE), m_pPipe(NULL), m_pTaskState(NULL)
    {};
    ~TTaskInfo();

    BOOL Init();
    TSection* AddNewSection(HTBSECTION a_hSection, LPCSTR a_pszName, DWORD a_dwAttrs, BOOL a_bReadOnly);
};

HTBSECTION  UTIL_GetSectionFromOutput(TTaskInfo *a_pTask, TCallParams &a_outParams);
void       TASK_RegisterTask(TTaskInfo *a_pTask);
void       TASK_UnregisterTask(TTaskInfo *a_pTask);
void       TASK_DeleteAll();  
TTaskInfo* TASK_GetCurrent(BOOL a_bResetState = TRUE);
void       TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszSection);
TSection*  SECTION_GetSection(HTBSECTION a_hSection);
void       SECTION_DeleteFromCache(HTBSECTION a_hSection, LPCSTR a_pszName);


#endif // _TBCORE_H_