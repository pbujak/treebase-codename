/* 
 * File:   TPageCache.h
 * Author: pabu
 *
 * Created on 25 January 2013, 10:52
 */

#ifndef TPAGECACHE_H
#define	TPAGECACHE_H

#include "syncobj.h"
#include "TreeBaseCommon.h"
#include "TGenericCache.h"
#include <utility>
#include <map>
#include <set>
#include <vector>

#define PAGE_COUNT  128
#define HASH_SIZE   97

namespace Debug
{
    class TPageDataTrace;
}

namespace DataPage{
    
class TPagePool
{
    HANDLE m_hHeap;
    
    inline TPagePool();
    inline ~TPagePool();
    
    static TPagePool s_instance;
public:
    inline static TPagePool& getInstance()
    {
        return s_instance;
    }
    void *getBlock();
    void releaseBlock(void* a_pvSeg);
};
    
typedef std::pair<FPOINTER, void*> PageInfo;


typedef struct _PAGE_ENTRY
{
    FPOINTER     fpPage;
    void        *pvBuff;
    bool         bDirty;
    RWLOCK       rwLock;
    int          nRefCount;
    int          nDiscardLockCount;
    bool         bRemoving;
}PAGE_ENTRY;


class TPageCache;
class TPageRef
{
    friend class TPageCache;
    
    FPOINTER     fpPage;
    void        *pvBuff;
    int          nAccess;
    bool         bDirty, bCanSetDirty;
    TPageCache  *pCache;
    _PAGE_ENTRY *pPage;
    int          nDiscardLockCount;
    bool         bLowCachingPriority;
    
public:
    enum {eNoAccess = 0, eRead, eReadWrite};
private:
    static void move(TPageRef& a_dest, TPageRef& a_src);
public:    
    inline TPageRef()
        : fpPage(0)
        , pvBuff(NULL)
        , nAccess(eNoAccess)
        , nDiscardLockCount(0)
        , pCache(NULL) 
        , bLowCachingPriority(false)
    {};
    inline TPageRef(const TPageRef& a_src)
        : fpPage(0)
        , pvBuff(NULL)
        , nAccess(eNoAccess)
        , nDiscardLockCount(0)
        , pCache(NULL) 
        , bLowCachingPriority(false)
    {
        move(*this, const_cast<TPageRef&>(a_src));
    }
    ~TPageRef();
    
    inline void* getBuffer()
    {
        return pvBuff;
    }
    void setDirty(bool a_bDirty = true);

    inline bool isDirty()
    {
        return bDirty;
    }
    
    inline bool lockDiscard(int a_nDelta)
    {
        nDiscardLockCount += a_nDelta;
        return true;
    }

    inline TPageRef& operator=(const TPageRef& a_src)
    {
        move(*this, const_cast<TPageRef&>(a_src));
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

    inline bool& lowCachingPriority()
    {
        return bLowCachingPriority;
    }
};

namespace{
    class TKeyMaker
    {
        FPOINTER m_fpPage;
    public:
        inline TKeyMaker(FPOINTER a_fpPage): m_fpPage(a_fpPage)
        {
        };
        inline TKeyMaker(PAGE_ENTRY *a_pEntry): 
            m_fpPage(a_pEntry->fpPage)
        {
        }

        inline DWORD getHash() const
        {
            return (DWORD)m_fpPage;
        }

        inline bool isMatching(PAGE_ENTRY *a_pEntry) const
        {
            return m_fpPage == a_pEntry->fpPage;
        };

        inline FPOINTER fpPage() const
        {
            return m_fpPage;
        }
        inline void fillNewEntry(PAGE_ENTRY *pEntry) const
        {
            pEntry->nRefCount = 0;
            pEntry->nDiscardLockCount = 0;
            pEntry->fpPage = fpPage();
            pEntry->pvBuff = DataPage::TPagePool::getInstance().getBlock();
            pEntry->bDirty = false;
        }
    };
}

typedef Util::TGenericCache<
    PAGE_ENTRY, 
    TKeyMaker, 
    PAGE_COUNT, 
    HASH_SIZE,
    Util::TMultiThreaded
> TPageCacheBase;

class TPageCache: public TPageCacheBase
{
    std::map<FPOINTER, void*> m_discardedDirtyPages;
    TSemaphore                m_semFreePages;
    int                       m_modificationKey;
    int                       m_nFreePageRefs, m_nFreePageWaiters;
    Debug::TPageDataTrace*    m_pTrace;

    inline TPageCache(const TPageCache& orig){};
    
protected:
    virtual void onDiscard(PAGE_ENTRY *a_pEntry);

public:
    TPageCache();
    virtual ~TPageCache();
    
    bool getPageForRead(FPOINTER a_fpPage, TPageRef& a_ref, bool a_bCanSetDirty = false);
    bool getPageForWrite(FPOINTER a_fpPage, TPageRef& a_ref, bool a_bCreate = false);
    void releasePage(TPageRef& a_ref);
    void popDiscardedDirtyPages(std::vector<PageInfo> &a_vecPages);
    void getDirtyPages(std::set<FPOINTER> &a_setPages);
    void removePage(FPOINTER a_fpPage);
    bool pageExists(FPOINTER a_fpPage);
    void* findDiscardedDirtyPage(FPOINTER a_fpPage);

    inline int getModificationKey()
    {
        TCSLock _lock(&cs());
        return m_modificationKey;
    }
private:
    bool addPageRef(PAGE_ENTRY *pPage, TCSLock &_lock);
};
    
} // namespace DataPage

template<>
class Util::TKeyMaker<DataPage::PAGE_ENTRY>
{
    FPOINTER m_fpPage;
public:
    inline Util::TKeyMaker<DataPage::PAGE_ENTRY>(FPOINTER a_fpPage): m_fpPage(a_fpPage)
    {
    };
    inline Util::TKeyMaker<DataPage::PAGE_ENTRY>(DataPage::PAGE_ENTRY *a_pEntry): 
        m_fpPage(a_pEntry->fpPage)
    {
    }

    inline DWORD getHash() const
    {
        return (DWORD)m_fpPage;
    }

    inline bool isMatching(DataPage::PAGE_ENTRY *a_pEntry) const
    {
        return m_fpPage == a_pEntry->fpPage;
    };

    inline FPOINTER fpPage() const
    {
        return m_fpPage;
    }
};


#endif	/* TPAGECACHE_H */

