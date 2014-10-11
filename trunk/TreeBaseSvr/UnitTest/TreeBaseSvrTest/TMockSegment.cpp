#include "StdAfx.h"
#include "TMockSegment.h"

TMockSegment::TMockSegment(void): m_nCount(0), m_nId(0), m_ndbId(0)
{
}

TMockSegment::~TMockSegment(void)
{
}

BOOL TMockSegment::M_FixStorage()
{
    return TRUE;
}

BOOL TMockSegment::Resize(long a_nPageCount)
{
    m_nCount = a_nPageCount;
    return TRUE;
}
    
int TMockSegment::GetPageCount()
{
    return m_nCount;
}

void* TMockSegment::GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page)
{
    if(a_nIndex >= m_nCount)
        return NULL;
    return m_pageSequence.getPageForRead((FPOINTER)a_nIndex);
}

void* TMockSegment::GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page)
{
    if(a_nIndex > m_nCount)
        return NULL;
    return m_pageSequence.getPageForWrite((FPOINTER)a_nIndex);
}

