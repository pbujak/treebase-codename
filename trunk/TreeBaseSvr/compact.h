#ifndef _COMPACT_H_
#define _COMPACT_H_

#if defined(UNIT_TEST)
#include "UnitTest\TreeBaseSvrTest\include_mock_storage.h"
#endif

#include "Storage.h"
#include "TGenericCache.h"

class TSegment;

struct SCompactPageInfo
{
    enum EMode
    {
        eRead, eWrite
    };

    FPOINTER  fpPage;
    void     *pvBuff;
    EMode     mode;
    Storage::TStorage *pStorage;
};

class TCompactBase
{
    long               m_ndbId;
    Storage::TStorage *m_pStorage;

    BOOL UpdateNextPage(FPOINTER a_fpFirst, FPOINTER a_fpOldNext, FPOINTER a_fpNewNext);
    BOOL UpdateSegmentDir(long a_nSegId, FPOINTER a_fpOldNext, FPOINTER a_fpNewNext);
    void SimpleMovePage(FPOINTER fpDest, FPOINTER fpSrc);
    void MovePage(FPOINTER fpDest, FPOINTER fpSrc);
    void MoveTable(FPOINTER fpDest, FPOINTER fpSrc);
    BOOL IsPageFree(FPOINTER a_fpPage, BOOL &a_bFree);
    DWORD GetPageSegmentId(FPOINTER a_fpPage);

    void* GetPage(FPOINTER a_fpPage, SCompactPageInfo::EMode a_mode);
    void Flush();
    //=============================================================================
    class KeyMaker
    {
        FPOINTER m_fpPage;
    public:
        inline KeyMaker(FPOINTER a_fpPage): m_fpPage(a_fpPage)
        {
        };
        inline KeyMaker(SCompactPageInfo *a_pEntry): 
            m_fpPage(a_pEntry->fpPage)
        {
        }

        inline DWORD getHash() const
        {
            return (DWORD)m_fpPage;
        }

        inline bool isMatching(SCompactPageInfo *a_pEntry) const
        {
            return m_fpPage == a_pEntry->fpPage;
        };

        inline void fillNewEntry(SCompactPageInfo *pEntry) const
        {
            pEntry->fpPage = m_fpPage;
        }
    };
    //=============================================================================
    class TLocalPageCache: public Util::TGenericCache<SCompactPageInfo,KeyMaker,8,11> 
    {
    public:
        inline SCompactPageInfo* findPage(FPOINTER a_fpPage)
        {
            return findEntry(a_fpPage);
        }
        inline SCompactPageInfo* addNewPage(FPOINTER a_fpPage)
        {
            return addNewEntry(a_fpPage);
        }
    };
    //=============================================================================
    TLocalPageCache m_localPageCache;      

protected:
    virtual BOOL IsOverTime() = 0;
    virtual void YieldControl() = 0;

public:
    BOOL Compact(long a_ndbId);
};



#endif // _COMPACT_H_