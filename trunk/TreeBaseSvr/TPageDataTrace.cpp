#include "stdafx.h"
#include "TPageDataTrace.h"
#include "util.h"
#include "TreeBaseCommon.h"
#include "Shared\TAbstractThreadLocal.h"
#include <assert.h>

using namespace Debug;

static TThreadLocal<bool>      s_bSuppressCheck;
static Util::TEnvSetting<bool> s_bPageDebug("TreeBase.Debug.Page");

TPageDataTrace::TPageDataTrace(void)
{
}

TPageDataTrace::~TPageDataTrace(void)
{
}

void TPageDataTrace::suppress()
{
    s_bSuppressCheck.Value() = true;
}

long TPageDataTrace::computeChecksum(void *a_pvData)
{
    long *ptr = (long*)a_pvData;
    long *ptrEnd = (long*)( (BYTE*)a_pvData + PAGESIZE );

    long result = 0;

    for( ; ptr < ptrEnd; ptr++)
    {
        result ^= *ptr;
    }
    return result;
}

TPageDataTrace* TPageDataTrace::getInstance()
{
    if(s_bPageDebug)
        return new TPageDataTrace();

    return NULL;
}

void TPageDataTrace::updatePageCRC(FPOINTER a_fpPage, void* a_pvData)
{
    if(s_bSuppressCheck.Value())
        return;

    long crc = computeChecksum(a_pvData);
    m_mapPageToCRC[a_fpPage] = crc;
}

void TPageDataTrace::setPageDiscard(FPOINTER a_fpPage, bool a_bDiscarded)
{
    if(s_bSuppressCheck.Value())
        return;

    if(a_bDiscarded)
    {
        m_discarded.insert(a_fpPage);
    }
    else
        m_discarded.erase(a_fpPage);
}

void TPageDataTrace::checkPageCRC(FPOINTER a_fpPage, void* a_pvData)
{
    if(s_bSuppressCheck.Value())
        return;

    if(m_mapPageToCRC.find(a_fpPage) == m_mapPageToCRC.end())
        return;

    bool bDiscarded = (m_discarded.find(a_fpPage) != m_discarded.end());
    long crc = computeChecksum(a_pvData);

    assert(m_mapPageToCRC[a_fpPage] == crc);
}

