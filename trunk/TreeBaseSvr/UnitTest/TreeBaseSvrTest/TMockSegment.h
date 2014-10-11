#pragma once
#include "TPageSequence.h"

namespace Storage
{
    class TAdvPageRef;
}


class TMockSegment
{
    TPageSequence m_pageSequence;
    int           m_nCount;
public:
    long          m_nId, m_ndbId;
public:
    TMockSegment(void);
public:
    ~TMockSegment(void);

    BOOL M_FixStorage(); 
public:
	BOOL Resize(long a_nPageCount);
    int GetPageCount();
    void* GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page);
    void* GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page);
};

