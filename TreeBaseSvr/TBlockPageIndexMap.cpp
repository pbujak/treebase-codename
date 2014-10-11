#include "stdafx.h"
#include "TBlockPageIndexMap.h"
#include <stack>

std::pair<int, FPOINTER> TBlockPageIndexMap::EmptyIndexAssoc;

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
TBlockPageIndexMap::TBlockPageIndexMap()
{
    memset(m_hashBlk, 0, sizeof(m_hashBlk));
}

//********************************************************************************
TBlockPageIndexMap::~TBlockPageIndexMap() 
{
}

//********************************************************************************
void TBlockPageIndexMap::reset()
{
    memset(m_hashBlk, 0, sizeof(m_hashBlk));
    TBlockPageIndexMapBase::reset();
}

//********************************************************************************
ASSOC_ENTRY* TBlockPageIndexMap::addNewAssoc(int a_dbId, int a_blkId, int a_nIndex, FPOINTER a_fpPage)
{
    ASSOC_ENTRY *pAssoc = addNewEntry(TKeyMaker(a_dbId, a_blkId, a_nIndex));

    // add to chain of decreasing page indexes
    int hashIdx = getBlockHashIndex(a_dbId, a_blkId);
    ASSOC_ENTRY** ppAssocBlk = &m_hashBlk[hashIdx];
    while(true)
    {
        ASSOC_ENTRY* pAssocBlk = *ppAssocBlk;

        if(pAssocBlk == NULL)
        {
            pAssoc->pLowerIndex = NULL;
            *ppAssocBlk = pAssoc;
            break;
        }
        else if(a_dbId == pAssocBlk->dbId &&
                a_blkId == pAssocBlk->blkId &&
                a_nIndex > pAssocBlk->nIndex)
        {
            pAssoc->pLowerIndex = pAssocBlk;
            *ppAssocBlk = pAssoc;
            break;
        }
        ppAssocBlk = &pAssocBlk->pLowerIndex;
    }

    // init data
    pAssoc->fpPage = a_fpPage;
    
    return pAssoc;
}

//********************************************************************************
void TBlockPageIndexMap::onDiscard(ASSOC_ENTRY *a_pAssoc)
{
    int hashIdx = getBlockHashIndex(a_pAssoc->dbId, a_pAssoc->blkId);
    _Util::removeFromHash(a_pAssoc, m_hashBlk, hashIdx, offsetof(ASSOC_ENTRY, pLowerIndex));
};


//********************************************************************************
void TBlockPageIndexMap::unlinkAssoc(ASSOC_ENTRY *a_pAssoc)
{
    int hashIdx = getBlockHashIndex(a_pAssoc->dbId, a_pAssoc->blkId);
    _Util::removeFromHash(a_pAssoc, m_hashBlk, hashIdx, offsetof(ASSOC_ENTRY, pLowerIndex));

    TBlockPageIndexMapBase::unlinkEntry(a_pAssoc);
}

//********************************************************************************
FPOINTER TBlockPageIndexMap::find(int a_dbId, int a_blkId, int a_nIndex)
{
    ASSOC_ENTRY *pEntry = findEntry(TKeyMaker(a_dbId, a_blkId, a_nIndex));
    if(pEntry)
    {
        bringEntryToFront(pEntry);
        return pEntry->fpPage;
    }
    return 0;
}

//********************************************************************************
FPOINTER& TBlockPageIndexMap::operator()(int a_dbId, int a_blkId, int a_nIndex)
{
    ASSOC_ENTRY *pEntry = findEntry(TKeyMaker(a_dbId, a_blkId, a_nIndex));
    if(pEntry)
    {
        bringEntryToFront(pEntry);
        return pEntry->fpPage;
    }
    else
    {
        pEntry = addNewAssoc(a_dbId, a_blkId, a_nIndex, 0);
        return pEntry->fpPage;
    }
}

//********************************************************************************
std::pair<int, FPOINTER> TBlockPageIndexMap::getLowerIndex(int a_dbId, int a_blkId, int a_nIndex)
{
    int          hashIdx = getBlockHashIndex(a_dbId, a_blkId);
    ASSOC_ENTRY *pEntry = m_hashBlk[hashIdx];

    while(pEntry)
    {
        if(pEntry->dbId == a_dbId && 
           pEntry->blkId == a_blkId &&
           pEntry->nIndex < a_nIndex)
        {
            return std::pair<int, FPOINTER>(pEntry->nIndex, pEntry->fpPage);
        }
        pEntry = pEntry->pLowerIndex;
    }
    return EmptyIndexAssoc;
}

//********************************************************************************
void TBlockPageIndexMap::removeBlockPages(int a_dbId, int a_blkId)
{
    std::stack<ASSOC_ENTRY*> stkAssoc;
    int          hashIdx = getBlockHashIndex(a_dbId, a_blkId);
    ASSOC_ENTRY *pEntry = m_hashBlk[hashIdx];

    while(pEntry)
    {
        if(pEntry->dbId == a_dbId && pEntry->blkId == a_blkId)
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