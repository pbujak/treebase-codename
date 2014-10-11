#pragma once
#include <utility>
#include "TGenericCache.h"

typedef struct _ASSOC_ENTRY
{
    _ASSOC_ENTRY *pNextFree;
    _ASSOC_ENTRY *pNextHash;
    _ASSOC_ENTRY *pNext;
    _ASSOC_ENTRY *pPrevious;
    _ASSOC_ENTRY *pLowerIndex;

    int           dbId, segId, nIndex;
    FPOINTER      fpPage;
}ASSOC_ENTRY;

typedef _ASSOC_ENTRY* ASSOC_ENTRY::*ASSOC_POINTER_OFFSET;

namespace
{
    //===============================================================
    class TKeyMaker
    {
    public:
        int m_dbId, m_segId, m_nIndex;
    public:
        //--------------------------------------------------------
        inline TKeyMaker(int a_dbId, int a_segId, int a_nIndex): 
            m_dbId(a_dbId), m_segId(a_segId), m_nIndex(a_nIndex)
        {
        };
        //--------------------------------------------------------
        inline TKeyMaker(ASSOC_ENTRY *a_pEntry)
        {
            m_dbId = a_pEntry->dbId;
            m_segId = a_pEntry->segId;
            m_nIndex = a_pEntry->nIndex;
        }

        //--------------------------------------------------------
        inline DWORD getHash() const
        {
            return DWORD( m_nIndex ^ (m_segId << 11) ^ (m_dbId << 22) );
        }

        //--------------------------------------------------------
        inline bool isMatching(ASSOC_ENTRY *a_pEntry) const
        {
            return (a_pEntry->dbId == m_dbId && 
                    a_pEntry->segId == m_segId &&
                    a_pEntry->nIndex == m_nIndex);
        };
        //--------------------------------------------------------
        inline void fillNewEntry(ASSOC_ENTRY *pEntry) const
        {
            pEntry->dbId = m_dbId;
            pEntry->segId = m_segId;
            pEntry->nIndex = m_nIndex;
        }


        //--------------------------------------------------------
    };

    //===============================================================
    enum ESizes
    {
        eAssocCount = 128,
        eHashSize = 97,
        eSegmentHashSize = 59
    };

    //===============================================================
    class TSegmentPageIndexMapTraits
    {
    public:
        typedef ASSOC_ENTRY EntryBrutto;

        inline static ASSOC_ENTRY* getNetto(ASSOC_ENTRY* a_pEntry)
        {
            return a_pEntry;
        }
        inline static ASSOC_ENTRY* getBrutto(ASSOC_ENTRY* a_pEntry)
        {
            return a_pEntry;
        }
        inline static void initEntries(ASSOC_ENTRY* a_pEntries, int a_nCount)
        {
            memset(a_pEntries, 0, sizeof(ASSOC_ENTRY) * a_nCount);
        }
    };
}


typedef Util::TGenericCache<
    ASSOC_ENTRY, 
    TKeyMaker, 
    eAssocCount, 
    eHashSize,
    Util::TSingleThreaded,
    TSegmentPageIndexMapTraits
> TSegmentPageIndexMapBase;


class TSegmentPageIndexMap: public TSegmentPageIndexMapBase
{
    ASSOC_ENTRY *m_hashSeg[eSegmentHashSize];
    
    inline TSegmentPageIndexMap(const TSegmentPageIndexMap& orig){};
    
    inline int getBlockHashIndex(int a_dbId, int a_segId)
    {
        int val = (a_dbId << 11) ^ (a_segId);
        return int(val % eSegmentHashSize);
    };
    ASSOC_ENTRY* addNewAssoc(int a_dbId, int a_segId, int a_nIndex, FPOINTER a_fpPage);
    void unlinkAssoc(ASSOC_ENTRY *a_pAssoc);
protected:
    virtual void onDiscard(ASSOC_ENTRY *a_pAssoc);
public:
    static std::pair<int, FPOINTER> EmptyIndexAssoc;

    TSegmentPageIndexMap(void);
    ~TSegmentPageIndexMap(void);
    void reset();

    FPOINTER&                operator()(int a_dbId, int a_segId, int a_nIndex);
    std::pair<int, FPOINTER> getLowerIndex(int a_dbId, int a_segId, int a_nIndex);
    void                     removeSegmentPages(int a_dbId, int a_segId);
    
    FPOINTER find(int a_dbId, int a_segId, int a_nIndex);
};
