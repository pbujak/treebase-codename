#include "stdafx.h"
#include "TSegmentPageIndexMap.h"
#include <stack>

std::pair<int, FPOINTER> TSegmentPageIndexMap::EmptyIndexAssoc;

//********************************************************************************
template<>
void Util::TCacheEntryOperations::initEntryData<ASSOC_ENTRY>(ASSOC_ENTRY *pEntry)
{
}

template<>
void Util::TCacheEntryOperations::freeEntryData<ASSOC_ENTRY>(ASSOC_ENTRY *pEntry)
{
    memset(pEntry, 0, sizeof(ASSOC_ENTRY));
}


//********************************************************************************
TSegmentPageIndexMap::TSegmentPageIndexMap()
{
    memset(m_hashSeg, 0, sizeof(m_hashSeg));
}

//********************************************************************************
TSegmentPageIndexMap::~TSegmentPageIndexMap() 
{
}

//********************************************************************************
void TSegmentPageIndexMap::reset()
{
    memset(m_hashSeg, 0, sizeof(m_hashSeg));
    TSegmentPageIndexMapBase::reset();
}

//********************************************************************************
ASSOC_ENTRY* TSegmentPageIndexMap::addNewAssoc(int a_dbId, int a_segId, int a_nIndex, FPOINTER a_fpPage)
{
    ASSOC_ENTRY *pAssoc = addNewEntry(TKeyMaker(a_dbId, a_segId, a_nIndex));

    // add to chain of decreasing page indexes
    int hashIdx = getBlockHashIndex(a_dbId, a_segId);
    ASSOC_ENTRY** ppAssocSeg = &m_hashSeg[hashIdx];
    while(true)
    {
        ASSOC_ENTRY* pAssocSeg = *ppAssocSeg;

        if(pAssocSeg == NULL)
        {
            pAssoc->pLowerIndex = NULL;
            *ppAssocSeg = pAssoc;
            break;
        }
        else if(a_dbId == pAssocSeg->dbId &&
                a_segId == pAssocSeg->segId &&
                a_nIndex > pAssocSeg->nIndex)
        {
            pAssoc->pLowerIndex = pAssocSeg;
            *ppAssocSeg = pAssoc;
            break;
        }
        ppAssocSeg = &pAssocSeg->pLowerIndex;
    }

    // init data
    pAssoc->fpPage = a_fpPage;
    
    return pAssoc;
}

//********************************************************************************
void TSegmentPageIndexMap::onDiscard(ASSOC_ENTRY *a_pAssoc)
{
    int hashIdx = getBlockHashIndex(a_pAssoc->dbId, a_pAssoc->segId);
    _Util::removeFromHash(a_pAssoc, m_hashSeg, hashIdx, offsetof(ASSOC_ENTRY, pLowerIndex));
};


//********************************************************************************
void TSegmentPageIndexMap::unlinkAssoc(ASSOC_ENTRY *a_pAssoc)
{
    int hashIdx = getBlockHashIndex(a_pAssoc->dbId, a_pAssoc->segId);
    _Util::removeFromHash(a_pAssoc, m_hashSeg, hashIdx, offsetof(ASSOC_ENTRY, pLowerIndex));

    TSegmentPageIndexMapBase::unlinkEntry(a_pAssoc);
}

//********************************************************************************
FPOINTER TSegmentPageIndexMap::find(int a_dbId, int a_segId, int a_nIndex)
{
    ASSOC_ENTRY *pEntry = findEntry(TKeyMaker(a_dbId, a_segId, a_nIndex));
    if(pEntry)
    {
        bringEntryToFront(pEntry);
        return pEntry->fpPage;
    }
    return 0;
}

//********************************************************************************
FPOINTER& TSegmentPageIndexMap::operator()(int a_dbId, int a_segId, int a_nIndex)
{
    ASSOC_ENTRY *pEntry = findEntry(TKeyMaker(a_dbId, a_segId, a_nIndex));
    if(pEntry)
    {
        bringEntryToFront(pEntry);
        return pEntry->fpPage;
    }
    else
    {
        pEntry = addNewAssoc(a_dbId, a_segId, a_nIndex, 0);
        return pEntry->fpPage;
    }
}

//********************************************************************************
std::pair<int, FPOINTER> TSegmentPageIndexMap::getLowerIndex(int a_dbId, int a_segId, int a_nIndex)
{
    int          hashIdx = getBlockHashIndex(a_dbId, a_segId);
    ASSOC_ENTRY *pEntry = m_hashSeg[hashIdx];

    while(pEntry)
    {
        if(pEntry->dbId == a_dbId && 
           pEntry->segId == a_segId &&
           pEntry->nIndex < a_nIndex)
        {
            return std::pair<int, FPOINTER>(pEntry->nIndex, pEntry->fpPage);
        }
        pEntry = pEntry->pLowerIndex;
    }
    return EmptyIndexAssoc;
}

//********************************************************************************
void TSegmentPageIndexMap::removeSegmentPages(int a_dbId, int a_segId)
{
    std::stack<ASSOC_ENTRY*> stkAssoc;
    int          hashIdx = getBlockHashIndex(a_dbId, a_segId);
    ASSOC_ENTRY *pEntry = m_hashSeg[hashIdx];

    while(pEntry)
    {
        if(pEntry->dbId == a_dbId && pEntry->segId == a_segId)
        {
            stkAssoc.push(pEntry);
        }
        pEntry = pEntry->pLowerIndex;
    }

    while(!stkAssoc.empty())
    {
        pEntry = stkAssoc.top();
        stkAssoc.pop();
        unlinkAssoc(pEntry); // not unlinkEntry
        recycleEntry(pEntry);
    }
}