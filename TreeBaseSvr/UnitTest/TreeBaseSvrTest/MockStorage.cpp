#include "StdAfx.h"
#include "MockStorage.h"
#include "TPageSequence.h"
#include "../../TSecurityAttributes.h"
#include "TreeBase.h"

using namespace Storage;
using namespace DataPage;

static Storage::TStorage *s_pStorage = NULL;

TMockPagePool TMockPagePool::s_instance;

//********************************************************************************
TMockPagePool::TMockPagePool(): m_nSegmentCount(0)
{
    m_hHeap = HeapCreate(0, 0x100000, 0x1800000);
}

//********************************************************************************
TMockPagePool::~TMockPagePool()
{
    HeapDestroy(m_hHeap);
}


//********************************************************************************
void *TMockPagePool::getBlock()
{
    m_nSegmentCount++;
    return HeapAlloc(m_hHeap, HEAP_ZERO_MEMORY, PAGESIZE);
}

//********************************************************************************
void TMockPagePool::releaseBlock(void* a_pvSeg)
{
    m_nSegmentCount--;
    HeapFree(m_hHeap, 0, a_pvSeg);
}

static Storage::TStorage s_staticStorage(FALSE);

//************************************************************************
Storage::TStorage* TASK_GetFixedStorage()
{
    if(s_pStorage == NULL)
    {
        s_pStorage = &s_staticStorage;
    }
    return s_pStorage;
}

//************************************************************************
void TASK_FixStorage(Storage::TStorage* a_pStorage)
{
    s_pStorage = a_pStorage;
}


//************************************************************************
TStorage::TStorage(BOOL a_bReadOnly): m_bReadOnly(a_bReadOnly), m_nFlags(0)
{
    m_pSeq = new TPageSequence();
}

//************************************************************************
TStorage::~TStorage(void)
{
    delete m_pSeq;
}

//************************************************************************
bool TStorage::getPageForRead(FPOINTER a_fpPage, DataPage::TMockPageRef& a_ref)
{
    a_ref.pvBuff = m_pSeq->getPageForRead(a_fpPage);
    a_ref.fpPage = a_fpPage;
    a_ref.nAccess = DataPage::TMockPageRef::eRead;
    return true;
}

//************************************************************************
bool TStorage::getPageForWrite(FPOINTER a_fpPage, DataPage::TMockPageRef& a_ref)
{
    a_ref.pvBuff = m_pSeq->getPageForWrite(a_fpPage);
    a_ref.fpPage = a_fpPage;
    a_ref.nAccess = DataPage::TMockPageRef::eReadWrite;

    setTopPointer(max(a_fpPage + PAGESIZE, m_fpTop));
    return true;
}

//************************************************************************
void TStorage::getRootSecurity(Security::TSecurityAttributes &a_attributes)
{
    a_attributes.getBase()->allAccessRights = TBACCESS_READWRITE;
}

//************************************************************************
void TSpinLock::Lock()
{
    if (m_nCount==0)
        EnterCriticalSection(&m_cs);
    InterlockedIncrement(&m_nCount);
};

//************************************************************************
void TSpinLock::Unlock()
{
    InterlockedDecrement(&m_nCount);
    if (m_nCount==0)
        LeaveCriticalSection(&m_cs);
};

//************************************************************************
TUseSpinLock::TUseSpinLock (TSpinLock *a_pSpinLock)
{
    if (a_pSpinLock)
    {
        a_pSpinLock->Lock();
        m_pSpinLock = a_pSpinLock;
    }
};

//************************************************************************
TUseSpinLock::~TUseSpinLock()
{
    if (m_pSpinLock)
        m_pSpinLock->Unlock();
};


//********************************************************************************
void DataPage::TMockPageRef::move(DataPage::TMockPageRef& a_dest, DataPage::TMockPageRef& a_src)
{
    a_dest.fpPage  = a_src.fpPage;
    a_dest.pvBuff  = a_src.pvBuff;
    a_dest.nAccess = a_src.nAccess;
    
    a_src.nAccess = TMockPageRef::eNoAccess;
    a_src.pvBuff  = NULL;
    a_src.fpPage  = 0;
}

//********************************************************************************
void DataPage::TMockPageRef::setDirty(bool a_bDirty)
{
}

//********************************************************************************
DataPage::TMockPageRef::~TMockPageRef()
{
}
