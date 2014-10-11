#pragma once

#if defined(UNIT_TEST)
#include "UnitTest\TreeBaseSvrTest\include_mock_storage.h"
#endif

#include "TreeBaseCommon.h"
#include "Storage.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TPageCache.h"
#include <map>
#include <set>
#include <afxtempl.h>

#define SUBTABLE_COUNT      16
#define PAGES_PER_SUBTABLE  (PAGESIZE / sizeof(PAGE_ENTRY))
#define PAGES_PER_TABLE     (PAGES_PER_SUBTABLE * SUBTABLE_COUNT)
#define TABLES_PER_DIR      (PAGESIZE / sizeof(FPOINTER))

typedef struct
{
    FPOINTER fpNext;
    DWORD    dwSegmentId;
}PAGE_ENTRY;


namespace Storage
{

class TAdvPageRef
{
    typedef std::map<FPOINTER, DataPage::TPageRef> TPageRefMap;
    typedef TPageRefMap::iterator                  TPageRefMapIter;

    //----------------------------------------------------------
    struct SPageState
    {
        int nAccess;
        FPOINTER fpPage;
        int nStateHash;
        TStorage *pStorage;

        inline SPageState(): nAccess(DataPage::TPageRef::eNoAccess), fpPage(0) {}
    };
    //----------------------------------------------------------
    class Data
    {
    public:
        inline Data()
            : m_nStateHash(0) 
            , m_pStorage(NULL)
        {};
        ~Data();

        static TThreadLocal<Data*> s_pData;

        inline static void* operator new(size_t a_size)
        {
            return TASK_MemAlloc(a_size);
        }
        inline static void operator delete(void *a_pvData)
        {
            TASK_MemFree(a_pvData);
        }
        static Data* getInstance();
        void release();
        void beginTrans();
        void endTrans();
        void releaseAllPages();
        void saveState(SPageState &a_state);
        void restoreState(SPageState &a_state);
    

        std::set<TStorage*> m_setTrans;
        TStorage           *m_pStorage;
        TPageRefMap         m_guardPageRefs;

        DataPage::TPageRef      m_pageRef;
        int                     m_nStateHash;
        int                     m_nRefCount;
    };
    //----------------------------------------------------------
    Data                *m_pData;
    Storage::TStorage   *m_pStorage;
    std::set<FPOINTER>   m_setLockedPages;
    SPageState           m_prevState;
private:
    void init(TStorage *a_pStorage = NULL);
public:
    TAdvPageRef();
    inline TAdvPageRef(TStorage *a_pStorage)
    {
        init(a_pStorage);
    }
    TAdvPageRef(int a_ndbId);
    ~TAdvPageRef();
    void lockGuardPageForRead(FPOINTER a_fpPage);
    void lockGuardPageForWrite(FPOINTER a_fpPage);
    void* getPageForRead(FPOINTER a_fpPage);
    void* getPageForWrite(FPOINTER a_fpPage);
    void releasePage();
    void releaseGuardPage(FPOINTER a_fpPage);
    void releaseAll(bool a_bEndTrans = true);
    FILEHEADER getHeader();
    void setHeader(FILEHEADER *a_pHeader);
    void releaseHeader();
    inline void beginTrans()
    {
        m_pData->beginTrans();
    }
    inline TStorage* getStorage()
    {
        return m_pStorage;
    }
    inline bool& lowCachingPriority()
    {
        return m_pData->m_pageRef.lowCachingPriority();
    }
    int& getStateHashRef();
};

void AdjustToSubtable(FPOINTER &a_fpTable, int &a_nPageIndex);

class TPageIterator
{
    TAdvPageRef &m_pageRef;

public:
    FPOINTER  m_fpTable, m_fpPage;
    int       m_nPageIndex;
    bool      m_bError;

public:
    inline TPageIterator(TAdvPageRef &a_pageRef): m_pageRef(a_pageRef), m_bError(false)
    {}
    bool setFirstPage(FPOINTER a_fpPage);
    bool setNextPage();
};


class TStoragePageAlloc
{
    inline TStoragePageAlloc(void) {}
public:
    static FPOINTER GetTableDirEntry(int a_nIndex, TAdvPageRef &a_xPageRef);
    static BOOL SetTableDirEntry(int a_nIndex, FPOINTER a_fpTable, TAdvPageRef &a_xPageRef);
    static BOOL GetLastPageTable(int *a_nDirIndex, FPOINTER *a_fpTable, TAdvPageRef &a_xPageRef);
    static BOOL GetNextPageCount(FPOINTER a_fpFirst, int &a_nPageCount);
    static BOOL GetNextPages(int a_idxBase, FPOINTER a_fpBase, int a_idxStart, 
                             CArray<FPOINTER,FPOINTER> &a_arrPages);
    static FPOINTER AddNewPage(FPOINTER a_fpFirst, DWORD a_dwSegmentId, BOOL a_bReserve);
    static FPOINTER CreateNewPage(DWORD a_dwSegmentId, BOOL a_bReserve);
    static BOOL FreeDataChain(FPOINTER a_fpFirst, BOOL a_bTruncate);

    static BOOL CreateNewTable(long a_nIndex);

    static BOOL GetTableAndPageIndex(FPOINTER a_fpPage, FPOINTER &a_fpTable, int &a_nPageIndex, TAdvPageRef &a_xPageRef);

    inline ~TStoragePageAlloc(void) {};
};

}