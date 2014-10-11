#if !defined(STORAGE_H)
#define STORAGE_H

#include "TPageCache.h"
#include "Actor.h"
#include "TDataFile.h"
#include "TTempFile.h"
#include "syncobj.h"
#include <stack>
#include <map>
#include "Shared\TAbstractThreadLocal.h"

namespace Security
{
    class TSecurityAttributes;
}

struct SEGID_F
{
    long ndbId;
    long nSegId;
};

#define SEGID_RESERVED   0x00FFFFFF
#define SEGID_TABLE      0x00FFFF01
#define SEGID_SEGMENTDIR 0x00FFFF02
#define SEGID_SECDATA    0x00FFFF03

#define PAGE_FAULT_FILE        0x1
#define PAGE_FAULT_CRC         0x2
#define PAGE_FAULT_NOT_EXISTS  0x3

namespace Storage
{
    enum EFlags {
        eFlag_DeleteOnly = 0x1,
        eFlag_EmptyPages = 0x2,
        eFlag_NeedCompact = 0x4
    };
    typedef struct _PAGE_REQUEST
    {
        HANDLE hSem;
        int    refCount;
    }PAGE_REQUEST;

    typedef struct _PAGE_FAULT
    {
        int   type;
        DWORD dwSysErrorCode;
        char  szFileName[MAX_PATH];
    }PAGE_FAULT;

    //========================================
    class TStorage
    {
        friend class TStorageHandlerTask;

        typedef std::map<FPOINTER, PAGE_REQUEST> TPageRequestMap;
        typedef TPageRequestMap::iterator        TPageRequestMapIter;
        typedef std::map<FPOINTER, PAGE_FAULT>   TPageFaultMap;
        typedef TPageFaultMap::iterator          TPageFaultMapIter;

        TSemaphore            m_semTrans;
        TCriticalSection      m_cs;
        TStorageHandlerTask  *m_pHandlerTask;
        TPageRequestMap       m_mapPageRequest, m_mapPageRequestUsed;
        TPageFaultMap         m_mapPageFault;
        std::stack<HANDLE>    m_semaphorePool;
        FPOINTER              m_fpTop;
        int                   m_nFlags; 
        FILEHEADER            m_header;
        int                   m_nTransCount, m_nWaitingTransCount;
        bool                  m_bFlush, m_bRequestFlush;
        int                   m_modificationKey;
        HANDLE                m_hSemTerminate;
    protected:
        bool                  m_bReadOnly;
        DataPage::TPageCache  m_pageCache;
    protected:
        TStorage(void);

        virtual void ensurePage(FPOINTER a_fpPage);
        void notifyPageLoaded(FPOINTER a_fpPage);
        HANDLE getSemaphoreFromPool();
        void throwPageException(FPOINTER a_fpPage);

        bool beginFlush(void);
        void endFlush(void);

        bool isModified()
        {
            TCSLock _lock(&m_cs);
            return m_modificationKey != m_pageCache.getModificationKey();
        }
        ~TStorage(void);
    public:
        static TStorage* open(LPCSTR a_pszPath);
        virtual void close();

        inline FPOINTER getTopPointer()
        {
            return m_fpTop;
        }
        inline void setTopPointer(FPOINTER a_fpTop)
        {
            InterlockedExchange((volatile LONG*)&m_fpTop, a_fpTop);
        }
        inline bool isReadOnly()
        {
            return m_bReadOnly;
        }
        inline int getFlags()
        {
            TCSLock _lock(&m_cs);
            return m_nFlags;
        }
        inline void setFlags(int a_nFlags, int a_nMask)
        {
            TCSLock _lock(&m_cs);
            m_nFlags = (m_nFlags & ~a_nMask) | a_nFlags;
        }

        void setPageFault(FPOINTER a_fpPage, PAGE_FAULT &a_fault);
        bool getPageForRead(FPOINTER a_fpPage, DataPage::TPageRef& a_ref);
        bool getPageForWrite(FPOINTER a_fpPage, DataPage::TPageRef& a_ref);
        inline void releasePage(DataPage::TPageRef& a_refPage)
        {
            m_pageCache.releasePage(a_refPage);
        }
        inline FILEHEADER getHeader()
        {
            TCSLock _lock(&m_cs);
            return m_header;
        }
        inline void setHeader(FILEHEADER *a_pHeader)
        {
            TCSLock _lock(&m_cs);
            m_header = *a_pHeader;
        }
    public:
        void beginTrans(void);
        void endTrans(void);
        virtual void flush();
        virtual void getRootSecurity(Security::TSecurityAttributes &a_secAttrs);
    };

    //========================================
    class TStorageHandlerTask: public Actor::TMessageQueueTask
    {
        friend class TStorage;

        CString   m_strPath;
        TStorage *m_pStorage;

        TDataFile  m_dataFile;
        TTempFile  m_tempFile;
        DWORD      m_dwFlushTime;

        TStorageHandlerTask(): Actor::TMessageQueueTask(1000), m_dwFlushTime(0) {};
    private:
        static TStorageHandlerTask* Start(TStorage *a_pStorage, LPCSTR a_pszPath, int a_nPriority);
        bool ReadSinglePage(FPOINTER a_fpPage, void *a_pvBuff, PAGE_FAULT *a_pFault);
        void UpdateDeleteOnlyStatus();
    public:
        virtual BOOL OnStart();
        virtual void OnTimer(DWORD a_dwTicks);

        void LoadPage(FPOINTER a_fpPage);
        void Flush();
        void Terminate();
    };
    //========================================
    class TStorageException: public CException
    {
        static TThreadLocal<TStorageException> s_exception;

    public:
        inline TStorageException(): CException(FALSE) {};  // no auto delete
        
        static void Throw(int a_nType, DWORD a_nSysErrorCode);

        int   m_nType;
        DWORD m_nSysErrorCode;
    };
    //========================================
    class TMemoryStorage: public TStorage
    {
        friend class TStorage;

        typedef std::map<FPOINTER, void*> TMapPages;
        typedef TMapPages::const_iterator TMapPagesIter;

        TCriticalSection  m_cs;
        TMapPages         m_mapPages;
    protected:
        TMemoryStorage(void);

        virtual void ensurePage(FPOINTER a_fpPage);
        virtual void close();
    public:
        virtual void flush() {};
        virtual void getRootSecurity(Security::TSecurityAttributes &a_secAttrs);
    };
    //========================================
    class TTempStorage: public TStorage
    {
        friend class TStorage;

        TCriticalSection  m_cs;
        TGenericTempFile  m_file;
        FPOINTER          m_fpFileTop;
    protected:
        TTempStorage(void);

        virtual void ensurePage(FPOINTER a_fpPage);
        virtual void close() {};
    public:
        virtual void flush() {};
        virtual void getRootSecurity(Security::TSecurityAttributes &a_secAttrs);
    };
}

template<>
inline void DeletePointer(Storage::TStorage *p)
{
    p->close();
};

#endif // STORAGE_H