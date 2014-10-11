/* 
 * File:   TMemoryPool.h
 * Author: pabu
 *
 * Created on 24 January 2013, 09:38
 */

#ifndef TMEMORYPOOL_H
#define	TMEMORYPOOL_H

#include <cstddef>

namespace Util
{

class TMemoryPool;

typedef struct _SEGMENT_HEADER
{
    union
    {
        _SEGMENT_HEADER *pSuper;
        TMemoryPool   *pPool;
    };
    _SEGMENT_HEADER *pNext;
    char data[1];
}SEGMENT_HEADER;

#define SEGMENT_HEADER_SIZE  offsetof(SEGMENT_HEADER, data)

class TMemoryPool 
{
    SEGMENT_HEADER *m_pFirst, *m_pFirstSuperSegment;
    int           m_nSizeSeg, m_nExtend;

    TMemoryPool(){};
    TMemoryPool(const TMemoryPool& orig){};
    
    void extend(int a_nSize);
public:
    TMemoryPool(int a_nSizeSeg, int a_nInitSegCount = 64, int a_nExtend = 32);
    
    void *getBlock();
    void releaseSegment(void *a_pvSeg);
    virtual ~TMemoryPool();
private:

};

} // Util

#endif	/* TMEMORYPOOL_H */

