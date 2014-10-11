#include "stdafx.h"
#include "PageRange.h"
#include "TPageCache.h"
#include "util.h"

#define ORDERED_BUFFER_COUNT 48
#define RANGE_MAX 16

using namespace Util;

//*************************************************
void TPageRange::reset()
{
    for(CachedPageMapIt it = m_mapPages.begin(); it != m_mapPages.end(); ++it)
    {
        DataPage::TPagePool::getInstance().releaseBlock(it->second);
    }
    m_mapPages.clear();
}

//*************************************************
FPOINTER TPageRange::getFirstPage()
{
    if(m_mapPages.empty())
        return 0;

    return m_mapPages.begin()->first;
}

//*************************************************
TPageRange::~TPageRange()
{
    reset();
}

//*************************************************
bool TPageRangeIterator::getFirstRange(TPageRange &a_range)
{
    m_baseHasNext = m_pBaseIterator->moveFirst();
    m_orderedBuffer.clear();
    m_it    = m_orderedBuffer.begin();
    m_itEnd = m_orderedBuffer.end();

    return getNextRange(a_range);
}

//*************************************************
bool TPageRangeIterator::getNextRange(TPageRange &a_range)
{
    a_range.reset();

    if(m_it == m_itEnd)
    {
        int count = 0;

        m_orderedBuffer.clear();

        while(m_baseHasNext && count < ORDERED_BUFFER_COUNT)
        {
            FPOINTER fpPage = m_pBaseIterator->getPage();
            void    *pvBuff = DataPage::TPagePool::getInstance().getBlock();
            m_pBaseIterator->getPageData(pvBuff);
            m_orderedBuffer[fpPage] = pvBuff;
            m_baseHasNext = m_pBaseIterator->moveNext();
            count++;
        }

        if(m_orderedBuffer.size() == 0)
            return false;

        m_it    = m_orderedBuffer.begin();
        m_itEnd = m_orderedBuffer.end();
    }

    //==============================================
    FPOINTER fpPrev = 0;

    for(int i = 0; i < RANGE_MAX; ++i)
    {
        if(m_it == m_itEnd)
            return true;

        if(a_range.m_mapPages.size() != 0 && m_it->first != (fpPrev + 1))  // Not subsequent
        {
            return true;
        }

        fpPrev = m_it->first;
        a_range.m_mapPages.insert( *(m_it++) );
    }

    return true;
}
