/* 
 * File:   TPageCache.cpp
 * Author: pabu
 * 
 * Created on 25 January 2013, 10:52
 */

#include "stdafx.h"
#include "TPageCache.h"
#include "TPageDataTrace.h"
#include <cstring>
#include <cstdlib>
#include <assert.h>

#define SIZE_384M  0x18000000
#define SIZE_4M    0x100000

#define HEAP_DEFAULT 0

using namespace DataPage;

TPagePool TPagePool::s_instance;

//********************************************************************************
TPagePool::TPagePool()
{
    m_hHeap = HeapCreate(HEAP_DEFAULT, SIZE_4M, SIZE_384M);
}

//********************************************************************************
TPagePool::~TPagePool()
{
    HeapDestroy(m_hHeap);
}


//********************************************************************************
void *TPagePool::getBlock()
{
    return HeapAlloc(m_hHeap, HEAP_ZERO_MEMORY, PAGESIZE);
}

//********************************************************************************
void TPagePool::releaseBlock(void* a_pvSeg)
{
    HeapFree(m_hHeap, HEAP_DEFAULT, a_pvSeg);
}

//********************************************************************************
void TPageRef::move(TPageRef& a_dest, TPageRef& a_src)
{
    if(a_dest.nAccess != TPageRef::eNoAccess)
        a_dest.pCache->releasePage(a_dest);
    
    a_dest.fpPage  = a_src.fpPage;
    a_dest.pvBuff  = a_src.pvBuff;
    a_dest.nAccess = a_src.nAccess;
    a_dest.bDirty  = a_src.bDirty;
    a_dest.bCanSetDirty = a_src.bCanSetDirty;
    a_dest.pCache = a_src.pCache;
    a_dest.pPage  = a_src.pPage;
    a_dest.nDiscardLockCount = a_src.nDiscardLockCount;
    
    a_src.bDirty = a_src.bCanSetDirty = false;
    a_src.nAccess = TPageRef::eNoAccess;
    a_src.pvBuff  = NULL;
    a_src.fpPage  = 0;
    a_src.pCache  = NULL;
    a_src.pPage   = NULL;
}

//********************************************************************************
void TPageRef::setDirty(bool a_bDirty)
{
    if(bCanSetDirty)
        bDirty = a_bDirty;
}

//********************************************************************************
TPageRef::~TPageRef()
{
    if(pCache && nAccess != TPageRef::eNoAccess)
        pCache->releasePage(*this);
}

//********************************************************************************
template<>
void Util::TCacheEntryOperations::onRecycleEntry<PAGE_ENTRY>(PAGE_ENTRY *pEntry)
{
    if(pEntry->pvBuff)
    {
        DataPage::TPagePool::getInstance().releaseBlock(pEntry->pvBuff);
    }
    pEntry->pvBuff = 0;
    pEntry->fpPage = 0;
    pEntry->bDirty = false;
    pEntry->bRemoving = false;
}



//********************************************************************************
TPageCache::TPageCache(): 
    m_semFreePages(0), 
    m_nFreePageRefs((PAGE_COUNT * 3) / 4),
    m_nFreePageWaiters(0),
    m_pTrace(Debug::TPageDataTrace::getInstance())
{
}

//********************************************************************************
TPageCache::~TPageCache() 
{
    if(m_pTrace)
        delete m_pTrace;
}

//********************************************************************************
void TPageCache::onDiscard(PAGE_ENTRY *a_pEntry)
{
    if(m_pTrace)
        m_pTrace->setPageDiscard(a_pEntry->fpPage, true);

    if(a_pEntry->bDirty)
    {
        m_discardedDirtyPages[a_pEntry->fpPage] = a_pEntry->pvBuff;
    }
    else
        TPagePool::getInstance().releaseBlock(a_pEntry->pvBuff);
    a_pEntry->pvBuff = NULL;
}

//********************************************************************************
bool TPageCache::addPageRef(PAGE_ENTRY *pPage, TCSLock &_lock)
{
    if(pPage->nRefCount == 0 && pPage->nDiscardLockCount == 0)
    {
        if(m_nFreePageRefs == 0)
        {
            m_nFreePageWaiters++;
            _lock.unlock();
            m_semFreePages.down();
            _lock.lock();
            m_nFreePageWaiters--;
            return false;
        }
        m_nFreePageRefs--;
    }
    pPage->nRefCount++;
    return true;
}

//********************************************************************************
bool TPageCache::getPageForWrite(FPOINTER a_fpPage, TPageRef& a_ref, bool a_bCreate)
{
    if(a_fpPage == a_ref.fpPage && a_ref.nAccess == TPageRef::eReadWrite)
        return true;
    
    bool bLockDiscard = false;

    if(a_ref.nAccess != TPageRef::eNoAccess)
    {
        if(a_fpPage == a_ref.fpPage)
            bLockDiscard = a_ref.lockDiscard(+1);

        releasePage(a_ref);
    }
    
    PAGE_ENTRY *pPage = NULL;
    //--------------------------------------------------------------
    // guarded segment
    do
    { 
        TCSLock _lock(&cs());
    
        pPage = findEntry(a_fpPage);

        if(pPage == NULL)
        {
            if(!a_bCreate)
                return false;

            pPage = addNewEntry(a_fpPage);
        }
        else
            bringEntryToFront(getBrutto(pPage));

        if(addPageRef(pPage, _lock))
        {
            a_ref.fpPage  = a_fpPage;
            a_ref.nAccess = TPageRef::eReadWrite;
            a_ref.pvBuff  = pPage->pvBuff;
            a_ref.bDirty  = true; //pPage->bDirty;
            a_ref.bCanSetDirty = true;
            a_ref.pCache  = this;
            a_ref.pPage   = pPage;
            a_ref.nDiscardLockCount = pPage->nDiscardLockCount;
            break;
        }
    }
    while(true);
    //----------------------------------------------------------
    if(bLockDiscard)
        a_ref.lockDiscard(-1);
    //----------------------------------------------------------
    RWLock_WriteLock(&pPage->rwLock);
    //----------------------------------------------------------
    {
        TCSLock _lock(&cs());

        if(m_pTrace)
            m_pTrace->checkPageCRC(a_fpPage, pPage->pvBuff);
    }

    return true;
}

//********************************************************************************
bool TPageCache::getPageForRead(FPOINTER a_fpPage, TPageRef& a_ref, bool a_bCanSetDirty)
{
    if(a_fpPage == a_ref.fpPage)
        return true;
    
    if(a_ref.nAccess != TPageRef::eNoAccess)
        releasePage(a_ref);
    
    PAGE_ENTRY *pPage = NULL;
    //--------------------------------------------------------------
    // guarded segment
    do
    {
        TCSLock _lock(&cs());
    
        pPage = findEntry(a_fpPage);
        if(pPage == NULL)
            return false;

        bringEntryToFront(getBrutto(pPage));

        if(addPageRef(pPage, _lock))
        {
            a_ref.fpPage  = a_fpPage;
            a_ref.nAccess = TPageRef::eRead;
            a_ref.pvBuff  = pPage->pvBuff;
            a_ref.bDirty  = pPage->bDirty;
            a_ref.bCanSetDirty = a_bCanSetDirty;
            a_ref.pCache  = this;
            a_ref.pPage   = pPage;
            a_ref.nDiscardLockCount = pPage->nDiscardLockCount;
            break;
        }
    }
    while(true);
    //----------------------------------------------------------
    RWLock_ReadLock(&pPage->rwLock);
    //----------------------------------------------------------
    {
        TCSLock _lock(&cs());

        if(m_pTrace)
            m_pTrace->checkPageCRC(a_fpPage, pPage->pvBuff);
    }
    return true;
}

//********************************************************************************
void TPageCache::releasePage(TPageRef& a_ref)
{
    if(a_ref.nAccess == TPageRef::eNoAccess)
        return;
    
    TCSLock _lock(&cs());
    
    PAGE_ENTRY *pPage = a_ref.pPage;  // no findPage - can be being removed
    assert(pPage && pPage->fpPage == a_ref.fpPage);
    
    if(a_ref.nAccess == TPageRef::eReadWrite && a_ref.bDirty)
    {
        m_modificationKey = rand();

        if(m_pTrace)
            m_pTrace->updatePageCRC(pPage->fpPage, pPage->pvBuff);
    }

    if(m_pTrace)
        m_pTrace->setPageDiscard(pPage->fpPage, false);

    pPage->bDirty = a_ref.bDirty;
    pPage->nDiscardLockCount = a_ref.nDiscardLockCount;

    if((--pPage->nRefCount) == 0 && pPage->nDiscardLockCount == 0)
    {
        m_nFreePageRefs++;
        if(m_nFreePageWaiters)
            m_semFreePages.up();
    }

    switch(a_ref.nAccess)
    {
        case TPageRef::eRead:
            RWLock_ReadUnlock(&pPage->rwLock);
            break;
        case TPageRef::eReadWrite:
            RWLock_WriteUnlock(&pPage->rwLock);
            break;
        default:
            assert(false);
    }
    if(pPage->nRefCount == 0 && pPage->bRemoving)
    {
        recycleEntry(getBrutto(pPage));
    }
    else if(canDiscardNow(pPage) && a_ref.bLowCachingPriority)
    {
        sendEntryToBack(getBrutto(pPage));
    }
    
    a_ref.nAccess = TPageRef::eNoAccess;
    a_ref.pvBuff  = NULL;
    a_ref.fpPage  = 0;
    a_ref.bDirty  = false;
    a_ref.pCache  = NULL;
    a_ref.pPage   = NULL;
    a_ref.nDiscardLockCount = 0;
    a_ref.bLowCachingPriority = false;
}

//********************************************************************************
void TPageCache::popDiscardedDirtyPages(std::vector<PageInfo> &a_vecPages)
{
    TCSLock _lock(&cs());

    a_vecPages.clear();
    a_vecPages.reserve(m_discardedDirtyPages.size());
    
    for(std::map<FPOINTER, void*>::const_iterator it = m_discardedDirtyPages.begin();
        it != m_discardedDirtyPages.end();
        ++it)
    {
        const PageInfo &dp = *it;
        a_vecPages.push_back(dp);
    }
    m_discardedDirtyPages.clear();
}

//********************************************************************************
void TPageCache::getDirtyPages(std::set<FPOINTER> &a_setPages)
{
    TCSLock _lock(&cs());
    
    a_setPages.clear();
    
    Iterator it = iterator();
    while(it.hasNext())
    {
        PAGE_ENTRY* pPage = it.next();
        if(pPage->bDirty)
        {
            a_setPages.insert(pPage->fpPage);
        }
    }
}

//********************************************************************************
void TPageCache::removePage(FPOINTER a_fpPage)
{
    TCSLock _lock(&cs());
    
    PAGE_ENTRY *pPage = findEntry(a_fpPage);
    if(pPage == NULL)
    {
        return;
    }
    unlinkEntry(getBrutto(pPage));
    if(pPage->nRefCount)
    {
        pPage->bRemoving = true;
    }
    else
        recycleEntry(getBrutto(pPage));

    m_modificationKey = rand();
}

//********************************************************************************
bool TPageCache::pageExists(FPOINTER a_fpPage)
{
    TCSLock _lock(&cs());

    if(findEntry(a_fpPage) != NULL)
        return true;

    return m_discardedDirtyPages.find(a_fpPage) != m_discardedDirtyPages.end();
}

//********************************************************************************
void* TPageCache::findDiscardedDirtyPage(FPOINTER a_fpPage)
{
    std::map<FPOINTER, void*>::const_iterator it = m_discardedDirtyPages.find(a_fpPage);
    if(it != m_discardedDirtyPages.end())
        return it->second;
    return NULL;
}

//********************************************************************************
template<>
inline void Util::TCacheEntryOperations::onInitEntry<DataPage::PAGE_ENTRY>(DataPage::PAGE_ENTRY *pEntry)
{
    RWLock_Initialize(&pEntry->rwLock);
};

//********************************************************************************
template<>
inline void Util::TCacheEntryOperations::onFinalRelease<DataPage::PAGE_ENTRY>(DataPage::PAGE_ENTRY *pEntry)
{
    RWLock_WriteLock(&pEntry->rwLock);
    RWLock_WriteUnlock(&pEntry->rwLock);
    RWLock_Delete(&pEntry->rwLock);
    DataPage::TPagePool::getInstance().releaseBlock(pEntry->pvBuff);
};

//********************************************************************************
template<>
inline bool Util::TCacheEntryOperations::canDiscardNow<DataPage::PAGE_ENTRY>(DataPage::PAGE_ENTRY *pEntry)
{
    return pEntry->nRefCount == 0 && pEntry->nDiscardLockCount == 0;
}

