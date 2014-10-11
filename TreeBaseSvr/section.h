#if !defined(_SECTION_H_)
#define _SECTION_H_

#include <afxtempl.h>
#include "DataMgr.h"
#include "atomic.h"
#include "syncobj.h"
#include "util.h"

class TTask;
class TLongBinary;
class TSectionSegmentIterator;
class TSectionSnapshot;

struct SEGID_F;

namespace Stream
{
    class TInputStream;
    class TOutputStream;
};

namespace Security
{
};

struct TSecCreateExtra
{
    long nSegId;
    BOOL bAddRef;
};

struct TSectionName
{
    CString  m_strName;
    long     m_nRefCount;   
};

class TSectionData
{
    THandle          m_hSemLock, m_hSemAttach, m_hSemAttachAndLock;
    TCriticalSection m_cs;
    int              m_nAttachWaiterCount, m_nLockWaiterCount, m_nAttachAndLockWaiterCount;
public:
	BOOL AssignSegment(SEGID_F a_segId);
	void AddName(LPCSTR a_pszName);
	void RemoveName(LPCSTR a_pszName);
    TSectionData();
    ~TSectionData();

    TSectionData* AddRef();
    void Release();
    BOOL Lock();
    void Unlock();
	BOOL TaskAttachAndLock();
	BOOL TaskAttach(BOOL a_bLock);
	void TaskDetach();

    bool                                m_bLock;
    int                                 m_nAttachCount;
    int                                 m_nRefCount;
    TAtomic32<TSectionData*>            m_pNext;
    SEGID_F                             m_segId; 
    TSectionSegment                       m_seg;
    CArray<TSectionName, TSectionName&> m_arrNames;
};

typedef BOOL (*FN_WaitForSegmentUnused)(SEGID_F a_nSegId);

typedef CArray<SEGID_F, SEGID_F&> TIDArrayBase;

class TIDArray: public TIDArrayBase
{
public:
    long M_Add(long a_ndbId, long a_nSegId);
};

class TSection;

struct SectionClose
{
    static void Close(TSection *a_pSection);
};

struct BlobSectionClose
{
    static void Close(TSection *a_pSection);
};

template<class X = SectionClose>
class TSectionRef
{
    TSection *m_pSection;
    BOOL      m_bOpen;

public:
    inline TSectionRef(TSection *a_pSection): m_pSection(a_pSection), m_bOpen(FALSE)
    {}

    inline BOOL Open(BOOL a_bLock = FALSE)
    {
        if(!m_bOpen) 
            m_bOpen = m_pSection->Open(a_bLock);

        return m_bOpen;
    }

    inline ~TSectionRef()
    {
        if(m_bOpen)
            X::Close(m_pSection);
    }
};

class TSection
{
    friend class Util::Internal::TSectionIterator;
    friend class Util::Internal::TSectionAccessor;
    friend class TTransactedLongBinaryOutputStream;

protected:
    long m_nOpenCount;
    long m_nLock;
    BOOL m_bReadOnly;
    BOOL m_bCanGetValue;

    TSectionSnapshot *m_pSnapshot;

    TSectionSegmentIterator *m_pIterator;
public:
    BOOL m_bSupportLabel;

    BOOL CreateSection(LPCSTR a_pszName, TSection *a_pSection, DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs, TSecCreateExtra *a_pExtra=NULL);
    CString       m_strName;
    long          m_ndbId;
    long          m_nSegId;
    TIDArray      m_arrPathIDs;  
    TSectionData *m_pData;
    DWORD         m_dwAttrs;  

    TSection(): 
        m_nOpenCount(0), 
        m_nLock(0),
        m_pData(NULL),
        m_bSupportLabel(TRUE),
        m_dwAttrs(0),
        m_pSnapshot(NULL),
        m_pIterator(NULL)
    {
        m_ndbId = 0;
        m_nSegId  = 0;
    };
    ~TSection();

    BOOL IsReadOnly();
    BOOL CanGetValue();
    BOOL WaitForSectionData(SEGID_F a_segId, LPCSTR a_pszName, TIDArray& a_arrIDs, BOOL a_bLock);
    void ReleaseSectionData(SEGID_F a_segId);
    int CreateSectionSegment(DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs);
protected:
    TSectionSegment* GetSegment();
    long FindSegmentItem(LPCSTR a_pszName, DWORD a_dwType);
    BOOL DeleteSegmentItem(LPCSTR a_pszName, long a_nSegId, BOOL a_bDeleteSegment=TRUE);
    BOOL RenameSegmentItem(LPCSTR a_pszOldName, LPCSTR a_pszNewName, long a_nType, FN_WaitForSegmentUnused a_pfnWaitForSegmentUnused=NULL);
    BOOL FindSectionEx(LPCSTR a_pszName, SEGID_F &a_segID);
    BOOL TranslateSegmentID(SEGID_F &a_segID);
    BOOL BeginValueOperation(LPCSTR a_pszSubName);
public:
    inline long FindSection(LPCSTR a_pszName);
    inline long FindLongBinary(LPCSTR a_pszName);
public:
	long GetLinkCount();
	BOOL RenameSection(LPCSTR a_pszOldName, LPCSTR a_pszNewName);
	BOOL DeleteSection(LPCSTR a_pszName);
	virtual BOOL ProcessBatch(LPVOID a_pvBatch);
    virtual BOOL CanModifyLabel();
    virtual BOOL Open(BOOL a_bLock = FALSE);
    virtual void Close();
    virtual BOOL Lock();
    virtual void Unlock();
    virtual void Release();
    BOOL InitialOpen(DWORD a_dwOpenMode);
	BOOL OpenSection(TSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode);
	static SEGID_F ResolvePath  (TSection *a_pBase, LPCSTR a_pszPath, CString &a_strFullPath, TIDArray *a_pIDsPath=NULL);
	static BOOL    SectionExists(TSection *a_pBase, LPCSTR a_pszPath, BOOL &a_bExist);

    LPVOID FetchValueByID(DWORD a_dwHash, WORD a_wOrder);
    LPVOID FetchValueByName(LPCSTR a_pszName, BOOL a_bSkipSections=TRUE);
    ITERATED_ITEM_ENTRY* GetFirstItems(TBASE_ITEM_FILTER *a_pFilter = NULL);
    ITERATED_ITEM_ENTRY* GetNextItems();

    BOOL CreateLink(TSection *a_pBase, LPCSTR a_pszPath, LPCSTR a_pszLinkName);

    Stream::TOutputStream* CreateLongBinary(LPCSTR a_pszName, TSecCreateExtra *a_pExtra=NULL);
    BOOL         DeleteExistingLongBinary(LPCSTR a_pszName);
    BOOL         DeleteLongBinary(LPCSTR a_pszName);
    BOOL         RenameLongBinary(LPCSTR a_pszOldName, LPCSTR a_pszNewName);
    BOOL         CreateLinkLB(TSection *a_pSource, LPCSTR a_pszValue, LPCSTR a_pszLinkName);

    BOOL GetSecurity(
        LPCSTR a_pszName,
        int    a_nSecurityInformationFlags, 
        std::auto_ptr<Util::Uuid> &a_owner,
        Util::Uuid                &a_protectionDomain,
        std::auto_ptr<Security::TSecurityAttributes> &a_attributes);

    BOOL SetSecurity(
        LPCSTR a_pszName,
        TBSECURITY_TARGET::Enum a_target,
        TBSECURITY_OPERATION::Enum a_operation,
        Security::TSecurityAttributes &a_secAttrs);

    Stream::TInputStream*  OpenLongBinary  (LPCSTR a_pszName);

    inline long GetOpenCount() { return m_nOpenCount; };

    DECLARE_TASK_MEMALLOC()
};

class TSharedSection: public TSection
{
    class RWLock
    {
        RWLOCK m_rwLock;
    public:
        inline RWLock()
        {
            RWLock_Initialize(&m_rwLock);
        }
        inline ~RWLock()
        {
            RWLock_Delete(&m_rwLock);
        }
        inline RWLOCK* operator&()
        {
            return &m_rwLock;
        }
    };

    static RWLock s_rwLock;
    
protected:
    bool     m_bLockExclusive;
    FPOINTER m_fpFirst;

    void GetFirstPage();
public:
    inline TSharedSection(): m_bLockExclusive(false) {};
    virtual void Release();
};

class TRootSection: public TSharedSection
{
public:
    TRootSection();
	virtual BOOL ProcessBatch(LPVOID a_pvBatch);
    virtual BOOL CanModifyLabel();
    virtual TLongBinary* CreateLongBinary(LPCSTR a_pszName, TSecCreateExtra *a_pExtra=NULL);
};

TSection* SECTION_CreateSection(TSection *a_pParent, LPCSTR a_pszName, DWORD a_dwAttrs, const Security::TSecurityAttributes *a_pSecAttrs);
TSection* SECTION_OpenSection(TSection *a_pBase, LPCSTR a_pszPath, DWORD a_dwOpenMode);
BOOL      SECTION_IsOpen(TSection *a_pSection);
void      SECTION_CloseSection(TSection *a_pSection);
void      SECTION_RemoveAll();
TSection* SECTION_FromHandle(HTBSECTION hSection);

Stream::TOutputStream* SECTION_CreateLongBinary(TSection *a_pSection, LPCSTR a_pszName);
Stream::TInputStream*  SECTION_OpenLongBinary(TSection *a_pSection, LPCSTR a_pszName);

inline void SectionClose::Close(TSection *a_pSection)
{
    a_pSection->Close();
}

inline void BlobSectionClose::Close(TSection *a_pSection)
{
    SECTION_CloseSection(a_pSection);
}

inline BOOL TSection::IsReadOnly()
{
    return m_bReadOnly;
};

inline BOOL TSection::CanGetValue()
{
    return m_bCanGetValue;
};

inline long TSection::FindSection(LPCSTR a_pszName)
{
    return FindSegmentItem(a_pszName, TBVTYPE_SECTION);
};
inline long TSection::FindLongBinary(LPCSTR a_pszName)
{
    return FindSegmentItem(a_pszName, TBVTYPE_LONGBINARY);
};


#endif //_SECTION_H_