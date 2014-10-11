#include "stdafx.h"
#include "InternalSection.h"
#include "section.h"

using namespace Util::Internal;

//******************************************************************************
TSectionIterator::TSectionIterator(TSection *a_pSection): 
    m_pSection(a_pSection), m_pEntry(NULL), m_nIndex(0)
{
    m_pSection->Open();

    m_pBlock = m_pSection->GetFirstItems();
    m_pEntry = m_pBlock;
}

//******************************************************************************
TSectionIterator::~TSectionIterator(void)
{
    if(m_pBlock)
        TASK_MemFree(m_pBlock);
    m_pSection->Close();

}

//******************************************************************************
ITERATED_ITEM_ENTRY* TSectionIterator::getNext()
{
    if(m_pBlock == NULL || m_pBlock->wSize == ITERATOR_END_ALL)
        return NULL;

    //-------------------------------------------
    if(m_pBlock->wSize == ITERATOR_END_BLOCK)
    {
        if(m_pBlock)
            TASK_MemFree(m_pBlock);
        m_pBlock = m_pSection->GetNextItems();
        m_pEntry = m_pBlock;
    }
    //-------------------------------------------
    ITERATED_ITEM_ENTRY *pResult = m_pEntry;
    m_pEntry = MAKE_PTR(ITERATED_ITEM_ENTRY*, m_pEntry, m_pEntry->wSize);
    //-------------------------------------------
    return pResult;
}

//******************************************************************************
TSectionAccessor::TSectionAccessor(TSection *a_pSection): 
    m_pSection(a_pSection), m_pEntry(NULL)
{
}

//******************************************************************************
TSectionAccessor::~TSectionAccessor()
{
    if(m_pEntry)
        TASK_MemFree(m_pEntry);
}

//******************************************************************************
TBVALUE_ENTRY* TSectionAccessor::getValue(LPCSTR a_pszName, DWORD a_dwType)
{
    TSectionRef<> sectionRef(m_pSection);

    if(!sectionRef.Open())
        return NULL;

    Storage::TAdvPageRef refPage(m_pSection->m_ndbId);

    TSectionSegment *pSegment = m_pSection->GetSegment();
    BPOINTER bptr = pSegment->FindValue(a_pszName);
    
    if(bptr == 0)
        return NULL;

    TSectionSegment::Ref<TBITEM_ENTRY> itemP = pSegment->GetItem(bptr, refPage);

    if(a_dwType != 0 && a_dwType != TBITEM_TYPE(itemP.ptr()))
        return NULL;

    if(m_pEntry != NULL)
        TASK_MemFree(m_pEntry);

    m_pEntry = (TBITEM_ENTRY *)TASK_MemAlloc(itemP->wSize);
    memcpy(m_pEntry, itemP.ptr(), itemP->wSize);
    
    return TBITEM_VALUE(m_pEntry);
}

//******************************************************************************
bool TSectionAccessor::addValue(LPCSTR a_pszName, DWORD a_dwType, TBVALUE_ENTRY *a_pValue, long a_cbSize)
{
    TSectionRef<> sectionRef(m_pSection);

    if(!sectionRef.Open(TRUE))
        return NULL;

    TSectionSegment *pSegment = m_pSection->GetSegment();

    return pSegment->AddValue(a_pszName, a_dwType, a_pValue, a_cbSize) != 0;
}
