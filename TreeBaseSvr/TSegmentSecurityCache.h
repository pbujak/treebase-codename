#ifndef _SEGMENTSECURITYCACHE_H_
#define _SEGMENTSECURITYCACHE_H_

#include "TGenericCache.h"
#include "SecurityManager.h"

namespace Security
{

struct SegmentSecurityEntry
{
    inline SegmentSecurityEntry(): ndbId(0), nSegId(0), pSecurity(NULL)
    {}
    ~SegmentSecurityEntry();

    int ndbId;
    int nSegId;
    SECURITY_ATTRIBUTES_BLOCK *pSecurity;
};

//===================================================================
class KeyMaker
{
    int m_ndbId;
    int m_nSegId;
public:
    inline KeyMaker(int a_ndbId, int a_nSegId): 
        m_ndbId(a_ndbId), m_nSegId(a_nSegId)
    {
    };
    inline KeyMaker(SegmentSecurityEntry *a_pEntry): 
        m_ndbId(a_pEntry->ndbId), m_nSegId(a_pEntry->nSegId)
    {
    };
    inline DWORD getHash() const
    {
        return (m_ndbId << 16) | m_nSegId;
    }

    inline bool isMatching(SegmentSecurityEntry *a_pEntry) const
    {
        return m_ndbId == a_pEntry->ndbId || m_ndbId == a_pEntry->ndbId;
    };

    inline void fillNewEntry(SegmentSecurityEntry *a_pEntry) const
    {
        a_pEntry->ndbId = m_ndbId;
        a_pEntry->nSegId = m_nSegId;
    };

};

//===================================================================
enum ESizes
{
    eCacheHashCount = 31,
    eCacheEntryCount = 20
};

//===================================================================
typedef Util::TGenericCache<
    SegmentSecurityEntry, 
    KeyMaker, 
    eCacheEntryCount, 
    eCacheHashCount,
    Util::TMultiThreaded
> TSegmentSecurityCacheBase;

//===================================================================
class TSegmentSecurityCache : public TSegmentSecurityCacheBase
{
    static TSegmentSecurityCache s_instance;

    TSegmentSecurityCache(void);

public:
    ~TSegmentSecurityCache(void);

    SECURITY_ATTRIBUTES_BLOCK* findSecurity(int a_ndbId, int a_nSegId);
    void addSecurity(int a_ndbId, int a_nSegId, SECURITY_ATTRIBUTES_BLOCK* a_pSecurity);
    void removeSecurity(int a_ndbId, int a_nSegId);
    void removeSecurity(const Util::Uuid &a_protectionDomain);

    inline static TSegmentSecurityCache& getInstance()
    {
        return s_instance;
    }
};

} // Security

#endif // _SEGMENTSECURITYCACHE_H_