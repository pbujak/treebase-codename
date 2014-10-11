#include "stdafx.h"
#include "defs.h"
#include "..\..\TPageCache.h"
#include "..\..\PageRange.h"
#include "..\..\Util.h"
#include <vector>

static FPOINTER s_fpPages[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 
                              30, 31, 36, 35, 34, 32, 33,
                              13, 14, 15, 16, 17, 18, 19, 20, 21,
                              87, 88, 89, 90, 91, 92, 93,
                              120, 121, 122, 123, 124, 125, 126, 131, 130, 129, 127, 128,
                              190, 180, 189, 181, 188, 182, 187, 183, 186, 185, 184,
                              100, 99, 98, 97, 96, 95, 94, 
                              -1 };

namespace
{

    class FakePageIterator: public Util::TCachedPageIterator
    {
        FPOINTER *m_pfp;
    public:
        virtual bool moveFirst()
        {
            m_pfp = s_fpPages;
            return true;
        }
        virtual bool moveNext()
        {
            ++m_pfp;
            return (*m_pfp != -1);
        };
        virtual FPOINTER getPage()
        {
            return *m_pfp;
        }
        virtual bool getPageData(void *a_pBuff)
        {
            memset(a_pBuff, *m_pfp, PAGESIZE);
            return true;
        };
    };

};

//***********************************************************************
TEST_ENTRY(PageCache_DataCorrectness, "Data Correctness")
{
    DataPage::TPageCache cache;
    DataPage::TPageRef   ref;

    for(int i = 0; i < 16; i++)
    {
        TEST_VERIFY(cache.getPageForWrite(i + 15, ref, TRUE));

        char *chBuff = (char*)ref.getBuffer();
        memset(chBuff, i, PAGESIZE);
    }
    cache.releasePage(ref);

    for(int i = 0; i < 16; i++)
    {
        TEST_VERIFY(cache.getPageForRead(i + 15, ref));

        char *chBuff = (char*)ref.getBuffer();
        for(int iChar = 0; iChar < PAGESIZE; iChar++)
        {
            TEST_VERIFY((*(chBuff++)) == i);
        }
    }

    return true;
}

//***********************************************************************
TEST_ENTRY(PageCache_Discard, "Discard")
{
    DataPage::TPageCache cache;
    DataPage::TPageRef   ref;

    for(int i = 0; i < PAGE_COUNT; i++)
    {
        TEST_VERIFY(cache.getPageForWrite(i + 15, ref, TRUE));

        char *chBuff = (char*)ref.getBuffer();
        memset(chBuff, i, PAGESIZE);
    }
    cache.releasePage(ref);

    TEST_VERIFY(cache.getPageForWrite(187, ref, TRUE));

    std::vector<DataPage::PageInfo> vecPages;
    cache.popDiscardedDirtyPages(vecPages);
    TEST_VERIFY(vecPages.size() == (PAGE_COUNT / 8));

    return true;
}

//***********************************************************************
static void testSingleRange(Util::TPageRange &a_range, std::set<FPOINTER> &a_setPages)
{
    FPOINTER fpLast = a_range.begin()->first;
    a_setPages.erase(fpLast);

    Util::TPageRange::const_iterator begin1 = a_range.begin();
    begin1++;

    for(Util::TPageRange::const_iterator it = begin1; it != a_range.end(); ++it)
    {
        TEST_VERIFY(it->first == fpLast + 1);
        fpLast = it->first;
        TEST_VERIFY(a_setPages.find(fpLast) != a_setPages.end());
        a_setPages.erase(fpLast);
    }
}

//***********************************************************************
TEST_ENTRY(PageCache_PageRange, "PageRange")
{
    std::set<FPOINTER> setPages;
    for(FPOINTER *pfp = s_fpPages; *pfp != -1; ++pfp)
    {
        setPages.insert(*pfp);
    }

    FakePageIterator fakePageIterator;

    Util::TPageRangeIterator rangeIterator(&fakePageIterator);

    Util::TPageRange range;

    bool hasNext = rangeIterator.getFirstRange(range);

    while(hasNext)
    {
        testSingleRange(range, setPages);
        hasNext = rangeIterator.getNextRange(range);
    }

    TEST_VERIFY(setPages.size() == 0);

    return true;
}