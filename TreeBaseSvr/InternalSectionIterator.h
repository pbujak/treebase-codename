#pragma once

#include "TreeBaseInt.h"

class TSection;

namespace Util
{

namespace Internal
{

    class TSectionIterator
    {
        int       m_nIndex;
        TSection *m_pSection;
        ID_TABLE *m_pIDTable;
        TBITEM_ENTRY *m_pEntry;
    public:
        TSectionIterator(TSection *a_pSection);
    public:
        ~TSectionIterator(void);

        TBITEM_ENTRY* getNext();
    };

}

}

