#ifndef _TGENERIC_CACHE_H_
#define _TGENERIC_CACHE_H_

#include <assert.h>
#include "syncobj.h"

namespace Util
{

template<typename _ENTRY>
struct SCacheEntryBrutto
{
    BYTE                       netto[sizeof(_ENTRY)];
    SCacheEntryBrutto<_ENTRY> *pNextFree;
    SCacheEntryBrutto<_ENTRY> *pNextHash;
    SCacheEntryBrutto<_ENTRY> *pNext;
    SCacheEntryBrutto<_ENTRY> *pPrevious;
};

template<typename _ENTRY>
class TCacheEntryTraits
{
public:
    typedef SCacheEntryBrutto<_ENTRY> EntryBrutto;

    inline static _ENTRY* getNetto(EntryBrutto *a_brutto)
    {
        return (_ENTRY*)a_brutto->netto;
    }
    inline static EntryBrutto* getBrutto(_ENTRY *a_brutto)
    {
        return (EntryBrutto*)a_brutto;
    }
    inline static void initEntries(EntryBrutto* a_pEntries, int a_nCount)
    {
        memset(a_pEntries, 0, sizeof(EntryBrutto) * a_nCount);
    }
};

template<typename _ENTRY>
class TKeyMaker;

template<typename _EntryBrutto>
class TCacheEntryUtil
{
public:
    typedef _EntryBrutto *PTR_ENTRY;

    //********************************************************************************
    static _EntryBrutto* &getRefNextHash(_EntryBrutto *a_pEntry, int a_offset)
    {
        return *(_EntryBrutto**)((BYTE*)a_pEntry + a_offset);
    }

    //********************************************************************************
    static void removeFromHash(_EntryBrutto *a_pEntry, 
                               _EntryBrutto *a_hash[], 
                               int     a_hashIdx,
                               int     a_offset)
    {
        _EntryBrutto**  ppEntry = &a_hash[a_hashIdx];
        _EntryBrutto*   pEntry = *ppEntry;

        while(pEntry)
        {
            if(pEntry == a_pEntry)
            {
                *ppEntry = getRefNextHash(pEntry, a_offset);
                return;
            }
            ppEntry = &getRefNextHash(pEntry, a_offset);
            pEntry = *ppEntry;
        }
    }

};

//=================================================================================
template<class _Threading>
class LockCS
{
    _Threading *m_pThreading;
public:
    LockCS(_Threading *a_pThreading): m_pThreading(a_pThreading)
    {
        m_pThreading->lockCS();
    }
    ~LockCS()
    {
        m_pThreading->unlockCS();
    }
};

//=================================================================================
class TSingleThreaded
{
    friend class LockCS<TSingleThreaded>;
protected:
    inline void lockCS() {};
    inline void unlockCS() {};
};

//=================================================================================
class TMultiThreaded
{
    friend class LockCS<TMultiThreaded>;

    TCriticalSection m_cs;
protected:
    inline void lockCS() 
    {
        EnterCriticalSection(&m_cs);
    };
    inline void unlockCS() 
    {
        LeaveCriticalSection(&m_cs);
    };

    inline TCriticalSection& cs()
    {
        return m_cs;
    };
};

//=================================================================================
class TCacheEntryOperations
{
public:
    template<typename _ENTRY>
    static void initEntryData(_ENTRY *a_pEntry);

    template<typename _ENTRY>
    static void freeEntryData(_ENTRY *a_pEntry);

    template<typename _ENTRY>
    static void onInitEntry(_ENTRY *a_pEntry)
    {
        initEntryData(a_pEntry);
    }

    template<typename _ENTRY>
    static bool canDiscardNow(_ENTRY *a_pEntry)
    {
        return true;
    }

    template<typename _ENTRY>
    static void onFinalRelease(_ENTRY *a_pEntry)
    {
        freeEntryData(a_pEntry);
    }

    template<typename _ENTRY>
    static void onRecycleEntry(_ENTRY *a_pEntry)
    {
        freeEntryData(a_pEntry);
        initEntryData(a_pEntry);
    }
};

template<int _Count>
struct SDiscardCount
{
    enum Enum
    {
        eValue = _Count
    };
};

template<>
struct SDiscardCount<0>
{
    enum Enum
    {
        eValue = 1
    };
};


//=================================================================================
template<typename _ENTRY, 
         class _KeyMaker, 
         int _EntryCount, 
         int _HashSize, 
         class _Threading = TSingleThreaded,
         class _Traits = TCacheEntryTraits<_ENTRY>
>
class TGenericCache: public _Threading
{
    typedef typename _Traits::EntryBrutto ENTRY_BRUTTO;

    inline TGenericCache(const TGenericCache& orig){};

    enum
    {
        eDiscardCount = SDiscardCount<_EntryCount / 8>::eValue
    };
private:
    //********************************************************************************
    void initialize()
    {
        memset(m_entries, 0, sizeof(m_entries));
        memset(m_hash,  0, sizeof(m_hash));
        
        for(int ctr = 0; ctr < _EntryCount; ctr++)
        {
            ENTRY_BRUTTO *pEntry = &m_entries[ctr];
            TCacheEntryOperations::onInitEntry(getNetto(pEntry));
            pEntry->pNextFree = m_pFreeList;
            m_pFreeList = pEntry;
        }
    }
    //********************************************************************************
    void shutdown()
    {
        lockCS();
        m_pFreeList = NULL;
        m_pFirst = NULL;
        m_pLast = NULL;
        memset(&m_hash, 0, sizeof(m_hash));
        unlockCS();
        
        for(int ctr = 0; ctr < _EntryCount; ctr++)
        {
            ENTRY_BRUTTO *pEntry = &m_entries[ctr];
            TCacheEntryOperations::onFinalRelease(getNetto(pEntry));
            pEntry->pNext = NULL;
            pEntry->pPrevious = NULL;
            pEntry->pNextHash = NULL;
            pEntry->pNextFree = NULL;
        }    
    }

protected:
    virtual void onDiscard(_ENTRY *a_pEntry) {};

protected:
    typedef TCacheEntryUtil<ENTRY_BRUTTO> _Util;

    ENTRY_BRUTTO  m_entries[_EntryCount];
    ENTRY_BRUTTO *m_hash[_HashSize];
    ENTRY_BRUTTO *m_pFreeList, *m_pFirst, *m_pLast;
        
    //********************************************************************************
    inline static ENTRY_BRUTTO* getBrutto(_ENTRY *a_pEntry)
    {
        return _Traits::getBrutto(a_pEntry);
    }

    //********************************************************************************
    inline static _ENTRY* getNetto(ENTRY_BRUTTO *a_pEntry)
    {
        return _Traits::getNetto(a_pEntry);
    }

    //********************************************************************************
    inline bool canDiscardNow(_ENTRY *a_pEntry)
    {
        return TCacheEntryOperations::canDiscardNow(a_pEntry);
    }

    //********************************************************************************
    void discard()
    {
        ENTRY_BRUTTO *pEntry = m_pLast;
        
        for(int ctr = 0; ctr < eDiscardCount; ctr++)
        {
            while(!canDiscardNow(getNetto(pEntry)))
            {
                pEntry = pEntry->pPrevious;
                assert(pEntry);
            }
            ENTRY_BRUTTO *pPrevious = pEntry->pPrevious;
            unlinkEntry(pEntry);
            onDiscard(getNetto(pEntry));
            recycleEntry(pEntry);
            pEntry = pPrevious;
        }
    }
    //********************************************************************************
    template<class _Matcher>
    void removeEntries(const _Matcher &a_matcher)
    {
        ENTRY_BRUTTO *pEntry = m_pLast;
        
        while(pEntry)
        {
            ENTRY_BRUTTO *pPrevious = pEntry->pPrevious;
            if(a_matcher.isMatching(getNetto(pEntry)))
            {
                unlinkEntry(pEntry);
                recycleEntry(pEntry);
            }
            pEntry = pPrevious;
        }
    }

    //********************************************************************************
    inline int getHashIndex(const _KeyMaker &a_keyMaker)
    {
        return int(a_keyMaker.getHash() % _HashSize);
    };

    //********************************************************************************
    void bringEntryToFront(ENTRY_BRUTTO *a_pEntry)
    {
        if(a_pEntry == m_pFirst)
        {
            return;
        }
        
        ENTRY_BRUTTO* &refPreviousOfNextP = (a_pEntry->pNext ? a_pEntry->pNext->pPrevious : m_pLast);
        
        refPreviousOfNextP = a_pEntry->pPrevious;
        a_pEntry->pPrevious->pNext = a_pEntry->pNext;
        
        m_pFirst->pPrevious = a_pEntry;
        a_pEntry->pNext = m_pFirst;
        a_pEntry->pPrevious = NULL;
        m_pFirst = a_pEntry;
    }

    //********************************************************************************
    void sendEntryToBack(ENTRY_BRUTTO *a_pEntry)
    {
        if(a_pEntry == m_pLast)
        {
            return;
        }
        
        ENTRY_BRUTTO* &refNextOfPreviousP = (a_pEntry->pPrevious ? a_pEntry->pPrevious->pNext : m_pFirst);
        
        refNextOfPreviousP = a_pEntry->pNext;
        a_pEntry->pNext->pPrevious = a_pEntry->pPrevious;
        
        m_pLast->pNext = a_pEntry;
        a_pEntry->pPrevious = m_pLast;
        a_pEntry->pNext = NULL;
        m_pLast = a_pEntry;
    }

    //********************************************************************************
    void unlinkEntry(ENTRY_BRUTTO *a_pEntry)
    {
        ENTRY_BRUTTO* &refPreviousOfNextP = (a_pEntry->pNext     ? a_pEntry->pNext->pPrevious : m_pLast);
        ENTRY_BRUTTO* &refNextOfPreviousP = (a_pEntry->pPrevious ? a_pEntry->pPrevious->pNext : m_pFirst);

        refPreviousOfNextP = a_pEntry->pPrevious;
        refNextOfPreviousP = a_pEntry->pNext;
        
        int hashIdx = getHashIndex(getNetto(a_pEntry));
        _Util::removeFromHash(a_pEntry, m_hash, hashIdx, offsetof(ENTRY_BRUTTO, pNextHash));
    }

    //********************************************************************************
    void relinkEntry(ENTRY_BRUTTO *a_pEntry)
    {
        int hashIdx = getHashIndex(a_pEntry);

        ENTRY_BRUTTO *pPrevEntry = m_hash[hashIdx];
        a_pEntry->pNextHash = pPrevEntry;
        m_hash[hashIdx] = a_pEntry;
    }


    //********************************************************************************
    void recycleEntry(ENTRY_BRUTTO *a_pEntry)
    {
        TCacheEntryOperations::onRecycleEntry(getNetto(a_pEntry));

        a_pEntry->pNextFree = m_pFreeList;
        m_pFreeList = a_pEntry;
            
    }

    //********************************************************************************
    _ENTRY* findEntry(const _KeyMaker &a_keyMaker)
    {
        int hashIdx = getHashIndex(a_keyMaker);
        
        ENTRY_BRUTTO *pEntry = m_hash[hashIdx];
        while(pEntry)
        {
            if(a_keyMaker.isMatching(getNetto(pEntry)))
            {
                bringEntryToFront(pEntry);
                return getNetto(pEntry);
            }
            pEntry = pEntry->pNextHash;
        }
        return NULL;
    }

    //********************************************************************************
    _ENTRY* addNewEntry(const _KeyMaker &a_keyMaker)
    {
        if(m_pFreeList == NULL)
        {
            discard();
        }
        ENTRY_BRUTTO *pEntry = m_pFreeList;
        m_pFreeList = pEntry->pNextFree;
        
        // Link pages in list
        ENTRY_BRUTTO *pNext = m_pFirst;
        pEntry->pNext = pNext;
        pEntry->pPrevious = NULL;
        m_pFirst = pEntry;
        
        if(pNext)
        {
            pNext->pPrevious = pEntry;
        }
        if(m_pLast == NULL)
        {
            m_pLast = m_pFirst;
        }
        
        int hashIdx = getHashIndex(a_keyMaker);
        
        pEntry->pNextHash = m_hash[hashIdx];
        m_hash[hashIdx] = pEntry;
        
        a_keyMaker.fillNewEntry(getNetto(pEntry));
        pEntry->pNextFree = NULL;
        return getNetto(pEntry);
    }

public:
    class Iterator
    {
        friend class TGenericCache;

        ENTRY_BRUTTO *m_pEntry;
    public:
        _ENTRY* next()
        {
            if(m_pEntry == NULL)
                return NULL;

            _ENTRY* pNetto = _Traits::getNetto(m_pEntry);
            m_pEntry = m_pEntry->pNext;
            return pNetto;
        }
        bool hasNext()
        {
            return m_pEntry != NULL;
        }
    };
    //********************************************************************************
    TGenericCache(): 
            m_pFirst(NULL), m_pLast(NULL), m_pFreeList(NULL)
    {
        initialize();
    }

    //********************************************************************************
    ~TGenericCache() 
    {
        shutdown();   
    }

    //********************************************************************************
    Iterator iterator()
    {
        Iterator it;
        it.m_pEntry = m_pFirst;
        return it;
    }
    //********************************************************************************
    void reset()
    {
        shutdown();
        initialize();
    }

};


};


#endif // _TGENERIC_CACHE_H_