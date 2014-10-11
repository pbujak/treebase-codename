#pragma once

#include "TreeBaseInt.h"

class TSection;
class TSectionSegmentIterator;

namespace Util
{

namespace Internal
{
    //=================================================
    class TSectionIterator
    {
        int                      m_nIndex;
        TSection                *m_pSection;
        ITERATED_ITEM_ENTRY     *m_pBlock, *m_pEntry;
    public:
        TSectionIterator(TSection *a_pSection);
    public:
        ~TSectionIterator(void);

        ITERATED_ITEM_ENTRY* getNext();
    };

    //=================================================
    class TSectionAccessor
    {
        TSection     *m_pSection;
        TBITEM_ENTRY *m_pEntry;
    public:
        TSectionAccessor(TSection *a_pSection);
    public:
        ~TSectionAccessor(void);

        TBVALUE_ENTRY* getValue(LPCSTR a_pszName, DWORD a_dwType = 0);
        bool           addValue(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize=-1);
    };

}

}

