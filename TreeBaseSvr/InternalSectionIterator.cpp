#include "stdafx.h"
#include "InternalSectionIterator.h"
#include "section.h"

using namespace Util::Internal;

//******************************************************************************
TSectionIterator::TSectionIterator(TSection *a_pSection): 
    m_pSection(a_pSection), m_pEntry(NULL), m_nIndex(0)
{
    m_pIDTable = m_pSection->GetIDTable();
    m_pSection->Open();
}

//******************************************************************************
TSectionIterator::~TSectionIterator(void)
{
    TASK_MemFree(m_pIDTable);
    if(m_pEntry)
        TASK_MemFree(m_pEntry);
    m_pSection->Close();
}

//******************************************************************************
TBITEM_ENTRY* TSectionIterator::getNext()
{
    if(m_pIDTable == NULL)
        return NULL;

    //-------------------------------------------
    if(m_nIndex++ < m_pIDTable->nCount)
    {
        Storage::TAdvPageRef refPage(m_pSection->m_ndbId);

        DWORD dwHash = m_pIDTable->nEntries[m_nIndex - 1].dwHash;
        WORD  wOrder = m_pIDTable->nEntries[m_nIndex - 1].wOrder;

        TSectionSegment *pSegment = m_pSection->GetSegment();
        BPOINTER bptr = pSegment->FindValueByID(dwHash, wOrder);
        
        TBITEM_ENTRY *pItem = pSegment->GetItemForRead(bptr, refPage);

        if(m_pEntry != NULL)
            TASK_MemFree(m_pEntry);
        m_pEntry = (TBITEM_ENTRY *)TASK_MemAlloc(pItem->wSize);
        memcpy(m_pEntry, pItem, pItem->wSize);

        return m_pEntry;
    }
    //-------------------------------------------
    return NULL;
}
