#if !defined(DATAMGR_H)
#define DATAMGR_H

#if defined(MOCK_SEGMENT)
#include "UnitTest\TreeBaseSvrTest\include_mock_segment.h"
#endif

#include "TStoragePageAlloc.h"
#include "SegmentMgr.h"
#include <afxtempl.h>
#include "TreeBaseInt.h"
#include "syncobj.h"

#define MAXDELETED 15

#define MANAGE_ACTIVE_PAGE() TManageActivePage xManageActivePage(this);

namespace Util
{
    namespace Internal
    {
        class TSectionIterator;
        class TSectionAccessor;
    }

    namespace Segment
    {
        bool readPage(TSegment* a_pSegment, int a_nPage, void *a_pvBuff);
        bool writePage(TSegment* a_pSegment, int a_nPage, void *a_pvBuff);
    }
}

namespace Security
{
    class TSecurityAttributes;
}

typedef struct
{
    TBITEM_ENTRY m_header;
    DWORD        m_dwDelCount;
    DWORD        m_dwAttrs;
    BPOINTER     m_pMap[1];
    // ...
    // BPOINTER footer
}SECTION_HEADER;


class TAdvancedSegment: public TSegment
{
    static TSemaphore            s_semEditModeLimit;

    BOOL                         m_bEditMode;
    CMap<int, int, void*, void*> m_mapEditPages;
    int                          m_nEditPageCount;
    int                          m_pageStateHash;

public:
    TAdvancedSegment(): m_bEditMode(FALSE), m_nEditPageCount(-1), m_pageStateHash(0) {};
    ~TAdvancedSegment();

    void* GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page);
    void* GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page);
	BOOL Resize(long a_nPageCount);
    int GetPageCount();

    BOOL Edit();
    BOOL Update();
    void CancelEdit();

    inline int& GetPageStateHashRef()
    {
        return m_pageStateHash;
    }
private:
    void   FreeEditData();
    LPVOID Edit_GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page);
    LPVOID Edit_GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page);
};

class TSectionSegment: public TAdvancedSegment
{
    friend class TSectionSegmentCompact;
    friend class Util::Internal::TSectionIterator;
    friend class Util::Internal::TSectionAccessor;

public:
    enum EAccess
    {
        eAccessNone = 0,
        eAccessRead, 
        eAccessWrite
    };

public:

    template<typename T>
    class Ref
    {
        template<typename U>
        friend class Ref;

        mutable T *m_ptr;
        BPOINTER  m_bptr;
        EAccess   m_access;
        TSectionSegment *m_pSegment;
        Storage::TAdvPageRef *m_pPageRef;
        mutable int           m_nSavedStateHash1, m_nSavedStateHash2;
        int                  *m_pStateHash1, *m_pStateHash2;

    public:
        inline Ref()
            : m_pPageRef(NULL)
            , m_pSegment(NULL)
            , m_bptr(0)
            , m_access(eAccessNone)
            , m_ptr(NULL)
            , m_nSavedStateHash1(0)
            , m_nSavedStateHash2(0)
        {
        }
        inline Ref(Storage::TAdvPageRef &a_xPageRef, TSectionSegment *a_pSegment, BPOINTER a_bptr, EAccess a_access)
            : m_pPageRef(&a_xPageRef)
            , m_pSegment(a_pSegment)
            , m_bptr(a_bptr)
            , m_access(a_access)
            , m_ptr(NULL)
            , m_nSavedStateHash1(0)
            , m_nSavedStateHash2(0)
        {
            m_pStateHash1 = &m_pSegment->GetPageStateHashRef();
            m_pStateHash2 = &m_pPageRef->getStateHashRef();
        }

        inline Ref(const Ref &a_source)
            : m_pPageRef(a_source.m_pPageRef)
            , m_pSegment(a_source.m_pSegment)
            , m_access(a_source.m_access)
            , m_ptr(a_source.ptr())
            , m_nSavedStateHash1(a_source.m_nSavedStateHash1)
            , m_nSavedStateHash2(a_source.m_nSavedStateHash2)
        {
            m_pStateHash1 = &m_pSegment->GetPageStateHashRef();
            m_pStateHash2 = &m_pPageRef->getStateHashRef();
        }
        
        template<typename U>
        inline Ref(const Ref<U> &a_source, int a_nDelta)
            : m_pPageRef(a_source.m_pPageRef)
            , m_pSegment(a_source.m_pSegment)
            , m_access(a_source.m_access)
            , m_ptr(NULL)
            , m_nSavedStateHash1(a_source.m_nSavedStateHash1)
            , m_nSavedStateHash2(a_source.m_nSavedStateHash2)
        {
            int nPage = BP_PAGE(a_source.m_bptr);
            int nOffset = BP_OFS(a_source.m_bptr);

            m_bptr = BP_MAKEBPTR(nPage, nOffset + a_nDelta);
            m_ptr = (T*)((BYTE*)a_source.ptr() + a_nDelta);

            m_pStateHash1 = &m_pSegment->GetPageStateHashRef();
            m_pStateHash2 = &m_pPageRef->getStateHashRef();
        }

        T* ptr() const
        {
            if(m_pPageRef == NULL)
                return NULL;

            if(m_ptr && 
               m_nSavedStateHash1 == *m_pStateHash1 && 
               m_nSavedStateHash2 == *m_pStateHash2)
                return m_ptr;

            int nPage = BP_PAGE(m_bptr);
            int nOffset = BP_OFS(m_bptr);

            void *pvPage = 
                m_access == TSectionSegment::eAccessWrite ? 
                m_pSegment->GetPageForWrite(nPage, *m_pPageRef) :
                m_pSegment->GetPageForRead(nPage, *m_pPageRef);

            m_nSavedStateHash1 = *m_pStateHash1;
            m_nSavedStateHash2 = *m_pStateHash2;
            m_ptr = (T*)((BYTE*)pvPage + nOffset);
            return m_ptr;
        }

        Ref& operator+=(int a_nDelta)
        {
            ptr();

            int nPage = BP_PAGE(m_bptr);
            int nOffset = BP_OFS(m_bptr);

            m_bptr = BP_MAKEBPTR(nPage, nOffset + a_nDelta);
            if(m_ptr)
                m_ptr = (T*)((BYTE*)m_ptr + a_nDelta);
            return *this;
        }

        inline int offset() const
        {
            return BP_OFS(m_bptr);
        }

        inline int page() const
        {
            return BP_PAGE(m_bptr);
        }

        inline BPOINTER bptr() const
        {
            return m_bptr;
        }

        inline void makeWritable()
        {
            m_access = TSectionSegment::eAccessWrite;
            m_ptr = 0;
        }

        inline T* operator->()
        {
            return ptr();
        }
        inline operator T*() const
        {
            return ptr();
        }
    };

    struct Iterator
    {
        Ref<TBITEM_ENTRY> refP;
        int nEffPageSize;
        int nMinPage;
        int nMaxPage;
        Storage::TAdvPageRef *pPageRef;
        EAccess eAccess;
    };
protected:
    class ValueKey;

    class NewValueKey
    {
        DWORD m_dwHash;
        WORD  m_wOrder;
        CString m_strName;
        enum EType {eNamed, eAnonymous} m_type;
    public:
        NewValueKey(LPCSTR a_pszName): 
            m_strName(a_pszName), 
            m_type(eNamed),
            m_wOrder(0)
        {
            m_dwHash = HASH_ComputeHash(m_strName);
        }
        NewValueKey(DWORD a_dwHash): 
            m_dwHash(a_dwHash),
            m_type(eAnonymous),
            m_wOrder(0)
        {
        }
        NewValueKey(const ValueKey & a_source)
        {
            switch(a_source.m_type)
            {
                case ValueKey::eByID:
                    m_dwHash = a_source.getHash();
                    m_wOrder = a_source.m_wOrder;
                    m_type = eAnonymous;
                    break;
                case ValueKey::eByName:
                    m_strName = a_source.m_strName;
                    m_type = eNamed;
                    break;
            }
        }
        inline DWORD getHash() const
        {
            return m_dwHash;
        };
        inline WORD getOrder() const
        {
            return m_wOrder;
        };
        inline LPCSTR getName() const
        {
            return m_strName;
        };
        inline bool isAnonymous() const
        {
            return m_type == eAnonymous;
        }
    };
protected:
    class ValueMatcher
    {
        DWORD m_dwHash;
    public:
        ValueMatcher(DWORD a_dwHash):
            m_dwHash(a_dwHash)
        {}
        inline DWORD getHash() const
        {
            return m_dwHash;
        };
        virtual bool isMatching(TBITEM_ENTRY *a_pEntry) const = 0;
    };

    class ValueKey: public ValueMatcher
    {
        friend class NewValueKey;

        WORD m_wOrder;
        CString m_strName;
        enum EType {eByName, eByID} m_type;

    public:
        ValueKey(LPCSTR a_pszName): 
            m_strName(a_pszName), 
            m_type(eByName),
            ValueMatcher(HASH_ComputeHash(a_pszName))
        {
        }
        inline ValueKey(DWORD a_dwHash, WORD a_wOrder): 
            ValueMatcher(a_dwHash), 
            m_wOrder(a_wOrder), 
            m_type(eByID)
        {}
        bool isMatching(TBITEM_ENTRY *a_pEntry) const;
        LPCSTR getName() const;
    };

protected:
    void          LinkEntry(Ref<TBITEM_ENTRY> & a_entryP);
    void          RelinkEntry(Ref<TBITEM_ENTRY> & a_entryP);
    void          UnlinkEntry(Ref<TBITEM_ENTRY> & a_entryP);
    BPOINTER      PrepareFreeEntry(WORD a_wNewSize);
    TBITEM_ENTRY* CreateItemEntry(const NewValueKey &a_newValueKey, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize);
	LPVOID        FetchValue(BPOINTER a_bpElem, BOOL a_bSkipSections=FALSE);
    void          RecycleEntry(Ref<TBITEM_ENTRY> & a_entryP);
public:

    static long CreateSegment(int a_ndbId, int a_segIdParent, DWORD a_dwAttrs, const Security::TSecurityAttributes &a_pSecAttrs);

    Ref<TBITEM_ENTRY> GetFirstItem(Iterator &a_iter, Storage::TAdvPageRef &a_xPageRef, EAccess a_access = eAccessRead, int a_nMinPage = -1, int a_nMaxPage = -1);
    Ref<TBITEM_ENTRY> GetNextItem(Iterator &a_iter);
	BOOL IsEmpty(BOOL &a_bEmpty);
	BOOL ProcessBatch(LPVOID a_pvBatch, BOOL a_bCanCompact = TRUE);
	LPVOID FetchValueByName(LPCSTR a_pszName, BOOL a_bSkipSections = TRUE);
    LPVOID FetchValueByID(DWORD a_dwHash, WORD a_wOrder);

	BOOL  Compact();
    DWORD GetDelCount();
    BPOINTER FindValue(const TSectionSegment::ValueMatcher &a_valueMatcher);
    BPOINTER AddValue(const NewValueKey &a_newValueKey, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize=-1);
    BOOL     DeleteValue(const ValueKey &a_valueKey);
    BPOINTER ModifyValue(const TSectionSegment::ValueKey &a_valueKey, LPCSTR a_pszNewName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize=-1);

    inline BPOINTER FindValueByID(DWORD a_dwHash, WORD a_wOrder)
    {
        return FindValue(ValueKey(a_dwHash, a_wOrder));
    }
    inline BPOINTER FindValue(const TSectionSegment::ValueKey &a_valueKey)
    {
        return FindValue((const TSectionSegment::ValueMatcher &)a_valueKey);
    }

    Ref<TBITEM_ENTRY> GetFirstItemOfPage(int a_nPage, Storage::TAdvPageRef &a_xPageRef, EAccess e_access = eAccessRead);
    Ref<TBITEM_ENTRY> GetItem(BPOINTER a_bpElem, Storage::TAdvPageRef &a_xPageRef, EAccess e_access = eAccessRead);
    Ref<SECTION_HEADER> GetHeader(Storage::TAdvPageRef &a_xPageRef, EAccess e_access = eAccessRead);

    ~TSectionSegment();
};

class TSectionSegmentIterator
{
    enum EConst{ 
        eBLOCK_SIZE = 7168, 
        eBLOCK_MARGIN = sizeof(_ITERATED_ITEM_ENTRY) 
    };

    int m_nPage;
    int m_nOffset;
    TItemFilterData m_filter;
    bool m_eof;

    void* m_pvResult;
    int m_nResultOffset;

private:
    bool pushEntry(TBITEM_ENTRY *a_pEntry);
    ITERATED_ITEM_ENTRY* getResult(bool a_hasMore);

public:
    TSectionSegmentIterator(TBASE_ITEM_FILTER *a_pFilter): 
      m_nPage(0), m_nOffset(0), m_pvResult(NULL), m_nResultOffset(0), m_eof(false)
    {
        m_filter.PrepareFilter(a_pFilter);
    }

    ITERATED_ITEM_ENTRY* getNextItems(TSectionSegment* a_pSegment);

    inline bool eof()
    {
        return m_eof;
    }
};

#endif //DATAMGR_H