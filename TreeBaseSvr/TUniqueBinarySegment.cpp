#include "stdafx.h"

#if defined(UNIT_TEST)
#define MOCK_SEGMENT
#endif

#include "TUniqueBinarySegment.h"
#include "TSecurityAttributes.h"
#include "Util.h"

//**********************************************************************************
static inline UNIQUE_BINARY_REF binaryRef(DWORD a_dwHash, WORD a_wOrder)
{
    UNIQUE_BINARY_REF bref = {a_dwHash, a_wOrder};
    return bref;
}

//**********************************************************************************
bool TUniqueBinarySegment::Matcher::isMatching(TBITEM_ENTRY *a_pEntry) const
{
    if(a_pEntry->dwHash != getHash())
        return false;

    BINARY_ENTRY* pBinEntry = getBinaryEntry(a_pEntry);

    return pBinEntry->cbSize == m_cbSize && memcmp(m_pvData, pBinEntry->data, m_cbSize) == 0;
}

//**********************************************************************************
TUniqueBinarySegment::TUniqueBinarySegment(void)
{
}

//**********************************************************************************
TUniqueBinarySegment::~TUniqueBinarySegment(void)
{
}

//**********************************************************************************
TUniqueBinarySegment::BINARY_ENTRY* TUniqueBinarySegment::getBinaryEntry(TBITEM_ENTRY *a_pEntry)
{
    if(a_pEntry == NULL)
        return NULL;

    TBVALUE_ENTRY* pValue = TBITEM_VALUE(a_pEntry);
    return (BINARY_ENTRY*)pValue;
}

//**********************************************************************************
TUniqueBinarySegment::Ref<TUniqueBinarySegment::BINARY_ENTRY> 
TUniqueBinarySegment::getBinaryEntryRef(TUniqueBinarySegment::Ref<TBITEM_ENTRY> &a_entry)
{
    if(a_entry.ptr() == NULL)
        return Ref<BINARY_ENTRY>();

    return Ref<BINARY_ENTRY>(a_entry, a_entry->iValue);
}

//**********************************************************************************
DWORD TUniqueBinarySegment::computeHash(void *a_pvData, int a_cbSize)
{
    BYTE  *byData = (BYTE*)a_pvData;
    
    DWORD dwHash = 0;

    a_cbSize -= (sizeof(DWORD) - sizeof(BYTE));

    for(int ctr = 0; ctr < a_cbSize; ctr++)
    {
        dwHash += *((DWORD*)(byData++));
    }
    return dwHash;
}

//**********************************************************************************
UNIQUE_BINARY_REF TUniqueBinarySegment::AddBinaryBlock(void *a_pvData)
{
    int cbSize = TASK_MemSize(a_pvData);

    Storage::TAdvPageRef xPageRef(m_ndbId);

    DWORD dwHash = computeHash(a_pvData, cbSize);

    BPOINTER bptr = FindValue(Matcher(dwHash, a_pvData, cbSize));

    if(bptr != 0)
    {
        Ref<TBITEM_ENTRY> itemP = GetItem(bptr, xPageRef, eAccessWrite);
        Ref<BINARY_ENTRY> binaryP = getBinaryEntryRef(itemP);
        binaryP->refCount++;
        return binaryRef(itemP->dwHash, itemP->wOrder);
    }
    else
    {
        Util::TTaskAutoPtr<BINARY_ENTRY> pBinary = (BINARY_ENTRY*)TASK_MemAlloc(sizeof(BINARY_ENTRY) + cbSize);
        memcpy(pBinary->data, a_pvData, cbSize);
        pBinary->refCount = 1;
        pBinary->cbSize = cbSize;

        bptr = AddValue(dwHash, TBVTYPE_BINARY, (TBVALUE_ENTRY*)pBinary.ptr(), TASK_MemSize(pBinary));
        Ref<TBITEM_ENTRY> itemP = GetItem(bptr, xPageRef, eAccessRead);

        return binaryRef(itemP->dwHash, itemP->wOrder);
    }
}

//**********************************************************************************
bool TUniqueBinarySegment::ChangeBinaryBlock(const UNIQUE_BINARY_REF & a_ref, void *a_pvData)
{
    int oldRefCount = 0;
    //----------------------------------------------------
    {
        Storage::TAdvPageRef xPageRef(m_ndbId);

        BPOINTER bptr = FindValue(ValueKey(a_ref.dwHash, a_ref.wOrder));

        if(bptr == 0)
            return false;

        Ref<BINARY_ENTRY> entryP = getBinaryEntryRef(GetItem(bptr, xPageRef));
        
        if(!entryP)
            return false;

        oldRefCount = entryP->refCount;
    }

    //----------------------------------------------------
    int cbSize = TASK_MemSize(a_pvData);

    Util::TTaskAutoPtr<BINARY_ENTRY> pBinary = (BINARY_ENTRY*)TASK_MemAlloc(sizeof(BINARY_ENTRY) + cbSize);
    memcpy(pBinary->data, a_pvData, cbSize);
    pBinary->refCount = oldRefCount;
    pBinary->cbSize = cbSize;

    return ModifyValue(ValueKey(a_ref.dwHash, a_ref.wOrder), NULL, TBVTYPE_BINARY, (TBVALUE_ENTRY*)pBinary.ptr(), TASK_MemSize(pBinary)) != 0;
}

//**********************************************************************************
void *TUniqueBinarySegment::GetBinaryBlock(const UNIQUE_BINARY_REF & a_ref)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    BPOINTER bptr = FindValue(ValueKey(a_ref.dwHash, a_ref.wOrder));

    if(bptr == 0)
        return NULL;

    Ref<BINARY_ENTRY> entryP = getBinaryEntryRef(GetItem(bptr, xPageRef));
    
    if(!entryP)
        return NULL;

    void *pvBinary = TASK_MemAlloc(entryP->cbSize);
    memcpy(pvBinary, entryP->data, entryP->cbSize);
    
    return pvBinary;
}

//**********************************************************************************
void TUniqueBinarySegment::DeleteBinaryBlock(const UNIQUE_BINARY_REF & a_ref)
{
    Storage::TAdvPageRef xPageRef(m_ndbId);

    BPOINTER bptr = FindValue(ValueKey(a_ref.dwHash, a_ref.wOrder));

    if(bptr == 0)
        return;

    Ref<BINARY_ENTRY> entryP = getBinaryEntryRef(GetItem(bptr, xPageRef, eAccessWrite));
    
    if((--entryP->refCount) == 0)
        DeleteValue(ValueKey(a_ref.dwHash, a_ref.wOrder));
}

//**********************************************************************************
template<typename T>
TUniqueBinarySegment::Ref<T> TUniqueBinarySegment::GetDataOfEntryRef(TUniqueBinarySegment::Ref<TBITEM_ENTRY> & a_entry)
{
    Ref<BINARY_ENTRY> binP = getBinaryEntryRef(a_entry);

    return Ref<T>(binP, offsetof(BINARY_ENTRY, data));
}

//**********************************************************************************
int TUniqueBinarySegment::GetDataOfEntrySize(TUniqueBinarySegment::Ref<TBITEM_ENTRY> & a_entry)
{
    Ref<BINARY_ENTRY> binP = getBinaryEntryRef(a_entry);

    return binP->cbSize;
}

//----------------------------------------------------------------------------------
template 
TUniqueBinarySegment::Ref<Security::SECURITY_ATTRIBUTES_BLOCK>
TUniqueBinarySegment::GetDataOfEntryRef(TUniqueBinarySegment::Ref<TBITEM_ENTRY> & a_entry);

