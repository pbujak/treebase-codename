#pragma once
#include <afxtempl.h>
#include "TreeBaseCommon.h"

typedef struct _tagFILEHEADER
{
    DWORD    m_fpSegmentDirPage;
    DWORD    m_fpSecDataPage;
    long     m_RootSectionSegId;
    long     m_LabelSectionSegId;
    long     m_TempLBSectionSegId;
    BOOL32   m_bDirty;
    BOOL32   m_bTrans;
    FPOINTER m_fpEnd;
    BOOL32   m_nPageTabDirCount;
}FILEHEADER;

struct SEGID_F
{
    long ndbId;
    long nSegId;
};

#define PAGES_OF_HEADER 1

#define SEGID_RESERVED 0x00FFFFFF
#define SEGID_TABLE    0x00FFFF01
#define SEGID_SEGMENTDIR 0x00FFFF02
#define SEGID_SECDATA  0x00FFFF03

namespace Security
{
    class TSecurityAttributes;
}

namespace DataPage
{
    
    class TMockPagePool
    {
        int    m_nSegmentCount;
        HANDLE m_hHeap;
        
        inline TMockPagePool();
        inline ~TMockPagePool();
        
        static TMockPagePool s_instance;
    public:
        inline static TMockPagePool& getInstance()
        {
            return s_instance;
        }
        void *getBlock();
        void releaseBlock(void* a_pvSeg);
    };

    class TMockPageRef
    {
    public:
        FPOINTER     fpPage;
        void        *pvBuff;
        int          nAccess;
        bool         m_lowCachingPriority;
    public:
        enum {eNoAccess = 0, eRead, eReadWrite};

        static void move(TMockPageRef& a_dest, TMockPageRef& a_src);
    public:    
        inline TMockPageRef(): fpPage(0), pvBuff(NULL), nAccess(eNoAccess), m_lowCachingPriority(false) 
        {};
        inline TMockPageRef(const TMockPageRef& a_src): fpPage(0), pvBuff(NULL), nAccess(eNoAccess)
        {
            move(*this, const_cast<TMockPageRef&>(a_src));
        }
        ~TMockPageRef();
        
        inline void* getBuffer()
        {
            return pvBuff;
        }
        void setDirty(bool a_bDirty = true);

        inline TMockPageRef& operator=(const TMockPageRef& a_src)
        {
            move(*this, const_cast<TMockPageRef&>(a_src));
            return *this;
        }

        inline FPOINTER getPage()
        {
            return fpPage;
        }

        inline int getAccess()
        {
            return nAccess;
        }
        inline void lockDiscard(int a_nLock) {};

        inline bool& lowCachingPriority()
        {
            return m_lowCachingPriority;
        }
    };
}

class TPageSequence;

namespace Storage
{
    enum EFlags {
        eFlag_DeleteOnly = 0x1,
        eFlag_EmptyPages = 0x2,
        eFlag_NeedCompact = 0x4
    };


    class TStorage
    {
        TPageSequence *m_pSeq;

        FILEHEADER m_hdr;
        FPOINTER   m_fpTop;
        BOOL       m_bReadOnly;
        DWORD      m_nFlags;

    public:
        TStorage(BOOL a_bReadOnly);
        ~TStorage(void);

        bool getPageForRead(FPOINTER a_fpPage, DataPage::TMockPageRef& a_ref);
        bool getPageForWrite(FPOINTER a_fpPage, DataPage::TMockPageRef& a_ref);
        inline void releasePage(DataPage::TMockPageRef& a_ref)
        {
            a_ref.fpPage  = 0;
            a_ref.nAccess = DataPage::TMockPageRef::eNoAccess;
            a_ref.pvBuff = NULL;
        }
        inline void beginTrans() {};
        inline void endTrans() {};
        inline FILEHEADER getHeader()
        {
            return m_hdr;
        }
        inline void setHeader(FILEHEADER *a_pHdr)
        {
            m_hdr = *a_pHdr;
        }
        inline void setTopPointer(FPOINTER a_fpTop)
        {
            m_fpTop = a_fpTop;
            m_hdr.m_fpEnd = a_fpTop;
        }
        inline FPOINTER getTopPointer()
        {
            return m_fpTop;
        }
        inline void flush() {};
        inline BOOL isReadOnly()
        {
            return m_bReadOnly;
        }
        inline int getFlags()
        {
            return m_nFlags;
        }
        inline void setFlags(int a_nFlags, int a_nMask)
        {
            m_nFlags = (m_nFlags & ~a_nMask) | a_nFlags;
        }
        void getRootSecurity(Security::TSecurityAttributes &a_attributes);
    };
};

Storage::TStorage* TASK_GetFixedStorage();
void TASK_FixStorage(Storage::TStorage* a_pStorage);

class TSpinLock
{
    CRITICAL_SECTION m_cs;
    long             m_nCount;
public:
    TSpinLock()
    {
        m_nCount = 0;
        InitializeCriticalSection(&m_cs);
    }
    void Lock();
    void Unlock();
};

class TUseSpinLock
{
    TSpinLock *m_pSpinLock;
public:
    TUseSpinLock (TSpinLock *a_pSpinLock = NULL);
    ~TUseSpinLock();
};

