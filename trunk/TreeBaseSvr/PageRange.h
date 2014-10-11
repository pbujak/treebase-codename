#ifndef _PAGE_RANGE_H_
#define _PAGE_RANGE_H_

#include <map>

namespace Util
{
    class TCachedPageIterator;

    typedef std::map<FPOINTER, void*>     CachedPageMap;
    typedef CachedPageMap::const_iterator CachedPageMapIt;

    //==============================================
    class TPageRange
    {
    public:
        typedef CachedPageMapIt const_iterator;

    public:
        CachedPageMap m_mapPages;

    public:
        void     reset();
        FPOINTER getFirstPage();
        
        inline int getCount()
        {
            return (int)m_mapPages.size();
        }

        inline CachedPageMapIt begin()
        {
            return m_mapPages.begin();
        }

        inline CachedPageMapIt end()
        {
            return m_mapPages.end();
        }

        ~TPageRange();
    };

    //==============================================
    class TPageRangeIterator
    {
        Util::TCachedPageIterator *m_pBaseIterator;
        CachedPageMap              m_orderedBuffer;
        CachedPageMapIt            m_it, m_itEnd;
        bool                       m_baseHasNext;
    public:
        TPageRangeIterator(Util::TCachedPageIterator *a_pBaseIterator)
            : m_pBaseIterator(a_pBaseIterator)
            , m_baseHasNext(false)
            , m_it(m_orderedBuffer.begin())
            , m_itEnd(m_orderedBuffer.end())
        {
        }

        bool getFirstRange(TPageRange &a_range);
        bool getNextRange(TPageRange &a_range);
    };
}

#endif // _PAGE_RANGE_H_