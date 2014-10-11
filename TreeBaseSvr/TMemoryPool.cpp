/* 
 * File:   TMemoryPool.cpp
 * Author: pabu
 * 
 * Created on 24 January 2013, 09:38
 */

#include "stdafx.h"
#include "TMemoryPool.h"
#include <assert.h>
#include <malloc.h>
#include <cstring>

using namespace Util;

TMemoryPool::TMemoryPool(int a_nSizeSeg, int a_nInitSegCount, int a_nExtend): 
        m_pFirst(NULL), 
        m_pFirstSuperSegment(NULL),
        m_nSizeSeg(a_nSizeSeg),
        m_nExtend(a_nExtend)
{
    assert(a_nSizeSeg > 0 && a_nInitSegCount > 0 && a_nExtend > 0);
    extend(a_nInitSegCount);
}


void TMemoryPool::extend(int a_nCount)
{
    int           nSize = a_nCount * (m_nSizeSeg + SEGMENT_HEADER_SIZE) + SEGMENT_HEADER_SIZE;
    SEGMENT_HEADER *pSuper = (SEGMENT_HEADER*)malloc(nSize);
    
    if(pSuper)
    {
        pSuper->pPool = this;
        pSuper->pNext = m_pFirstSuperSegment;
        m_pFirstSuperSegment = pSuper;
        
        SEGMENT_HEADER *pSeg = (SEGMENT_HEADER *)pSuper->data;
        
        for(int ctr = 0; ctr < a_nCount; ctr++)
        {
            pSeg->pNext = m_pFirst;
            pSeg->pSuper = pSuper;
            m_pFirst = pSeg;
            
            pSeg = (SEGMENT_HEADER *)((char*)pSeg + m_nSizeSeg + SEGMENT_HEADER_SIZE);
        }
    }
}

void *TMemoryPool::getBlock()
{
    if(m_pFirst == NULL)
    {
        extend(m_nExtend);
    }
    if(m_pFirst == NULL)  //extending failed - not enough memory
    {
        return NULL;
    }
    SEGMENT_HEADER *pSeg = m_pFirst;
    m_pFirst = pSeg->pNext;

    memset(pSeg->data, 0, m_nSizeSeg);
    return pSeg->data;
}

void TMemoryPool::releaseSegment(void *a_pvSeg)
{
    if(a_pvSeg)
    {
        SEGMENT_HEADER *pSeg = (SEGMENT_HEADER *)((char*)a_pvSeg - SEGMENT_HEADER_SIZE);

        assert(pSeg->pSuper->pPool == this);
        pSeg->pNext = m_pFirst;
        m_pFirst = pSeg;
    }
}

TMemoryPool::~TMemoryPool() 
{
    while(m_pFirstSuperSegment)
    {
        SEGMENT_HEADER *pSuper = m_pFirstSuperSegment;
        m_pFirstSuperSegment = pSuper->pNext;
        free(pSuper);
    }
}
