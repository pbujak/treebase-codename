#include "stdafx.h"
#include "TSegmentSecurityCache.h"
#include "TTask.h"

using namespace Security;

//============================================================================
namespace
{
    class ProtectionDomainMatcher
    {
        Util::Uuid m_domain;
    public:
        ProtectionDomainMatcher(const Util::Uuid &a_domain): m_domain(a_domain)
        {}

        inline bool isMatching(SegmentSecurityEntry *a_pEntry) const
        {
            return a_pEntry->pSecurity && a_pEntry->pSecurity->base.protectionDomain == m_domain;
        };
    };
}

//============================================================================
TSegmentSecurityCache TSegmentSecurityCache::s_instance;

//*****************************************************************************
SegmentSecurityEntry::~SegmentSecurityEntry()
{
    if(pSecurity)
        free(pSecurity);
}

//*****************************************************************************
TSegmentSecurityCache::TSegmentSecurityCache(void)
{
}

//*****************************************************************************
TSegmentSecurityCache::~TSegmentSecurityCache(void)
{
}

//*****************************************************************************
SECURITY_ATTRIBUTES_BLOCK* TSegmentSecurityCache::findSecurity(int a_ndbId, int a_nSegId)
{
    Util::LockCS<Util::TMultiThreaded> _lockCS(this);

    SegmentSecurityEntry* pEntry = findEntry(KeyMaker(a_ndbId, a_nSegId));

    if(pEntry == NULL)
        return NULL;

    int cbSize = _msize(pEntry->pSecurity);
    SECURITY_ATTRIBUTES_BLOCK *pBlock = (SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(cbSize);
    memcpy(pBlock, pEntry->pSecurity, cbSize);
    return pBlock;
}

//*****************************************************************************
void TSegmentSecurityCache::addSecurity(int a_ndbId, int a_nSegId, SECURITY_ATTRIBUTES_BLOCK* a_pSecurity)
{
    Util::LockCS<Util::TMultiThreaded> _lockCS(this);

    SegmentSecurityEntry *pEntry = addNewEntry(KeyMaker(a_ndbId, a_nSegId));

    int cbSize = TASK_MemSize(a_pSecurity);
    pEntry->pSecurity = (SECURITY_ATTRIBUTES_BLOCK *)malloc(cbSize);
    memcpy(pEntry->pSecurity, a_pSecurity, cbSize);
}

//*****************************************************************************
void TSegmentSecurityCache::removeSecurity(int a_ndbId, int a_nSegId)
{
    removeEntries(KeyMaker(a_ndbId, a_nSegId));
}

//*****************************************************************************
void TSegmentSecurityCache::removeSecurity(const Util::Uuid &a_protectionDomain)
{
    removeEntries(ProtectionDomainMatcher(a_protectionDomain));
}

//********************************************************************************
template<>
void Util::TCacheEntryOperations::initEntryData<SegmentSecurityEntry>(SegmentSecurityEntry *pEntry)
{
    new(pEntry) SegmentSecurityEntry();
};

//********************************************************************************
template<>
void Util::TCacheEntryOperations::freeEntryData<SegmentSecurityEntry>(SegmentSecurityEntry *pEntry)
{
    pEntry->SegmentSecurityEntry::~SegmentSecurityEntry();
};
