#include "stdafx.h"
#include "Storage.h"
#include "TSystemStatus.h"
#include "TPageDataTrace.h"
#include "TSecurityAttributes.h"
#include "SecurityManager.h"
#include "PageRange.h"
#include <vector>
#include <set>
#include <afxwin.h>

using namespace Storage;

TThreadLocal<TStorageException> TStorageException::s_exception;

//====================================================================
namespace
{
    class TCombinedPageIterator: public Util::TCachedPageIterator
    {
        typedef std::set<FPOINTER>       SetPages;
        typedef SetPages::const_iterator SetPagesIterator;

        std::set<FPOINTER>        &m_setDirtyPages;
        TTempFile                 *m_pTempFile; 
        DataPage::TPageCache      *m_pPageCache;
        Util::TCachedPageIterator *m_pTempFileIt;
        SetPagesIterator           m_itDirtyPages;

        enum EState{eDirtyPages, eTempFile} m_state;

    private:
        //========================================================
        bool moveFirstInTempFile()
        {
            bool more = m_pTempFileIt->moveFirst();
            while(more)
            {
                FPOINTER fpPage = m_pTempFileIt->getPage();
                // skip existing in dirty set
                if(fpPage != FPOINTER(-1) && m_setDirtyPages.find(fpPage) == m_setDirtyPages.end())
                    return true;

                more = m_pTempFileIt->moveNext();
            }
            return false;
        }

    public:
        //========================================================
        TCombinedPageIterator(std::set<FPOINTER> &a_setDirtyPages,
                              DataPage::TPageCache *a_pPageCache,
                              TTempFile *a_pTempFile)
            : m_setDirtyPages(a_setDirtyPages), 
              m_pTempFile(a_pTempFile),
              m_pPageCache(a_pPageCache)
        {
            m_pTempFileIt = m_pTempFile->getIterator();
        }
        //========================================================
        ~TCombinedPageIterator()
        {
            delete m_pTempFileIt;
        }

        //========================================================
        virtual bool moveFirst()
        {
            if(!m_setDirtyPages.empty())
            {
                m_state = eDirtyPages;
                m_itDirtyPages = m_setDirtyPages.begin();
                return true;
            }
            else
            {
                m_state = eTempFile;
                return moveFirstInTempFile();
            }
            assert(false);
            return false;
        }
        //========================================================
        virtual bool moveNext()
        {
            switch(m_state)
            {
                case eDirtyPages:
                {
                    if((++m_itDirtyPages) != m_setDirtyPages.end())
                        return true;

                    m_state = eTempFile;
                    return moveFirstInTempFile();
                }
                case eTempFile:
                {
                    while(m_pTempFileIt->moveNext())
                    {
                        FPOINTER fpPage = m_pTempFileIt->getPage();
                        // skip existing in dirty set
                        if(fpPage != FPOINTER(-1) && m_setDirtyPages.find(fpPage) == m_setDirtyPages.end())
                            return true;
                    }
                    return false;
                }
            }
            assert(false);
            return false;
        }
        //========================================================
        virtual FPOINTER getPage()
        {
            switch(m_state)
            {
                case eDirtyPages:
                {
                    return *m_itDirtyPages;
                }
                case eTempFile:
                {
                    return m_pTempFileIt->getPage();
                }
            }
            assert(false);
            return -1;
        }
        //========================================================
        virtual bool getPageData(void *a_pvBuff)
        {
            switch(m_state)
            {
                case eDirtyPages:
                {
                    DataPage::TPageRef pageRef;
                    m_pPageCache->getPageForRead(*m_itDirtyPages, pageRef);
                    memcpy(a_pvBuff, pageRef.getBuffer(), PAGESIZE);
                    return true;
                }
                case eTempFile:
                {
                    return m_pTempFileIt->getPageData(a_pvBuff);
                }
            }
            assert(false);
            return false;
        }
    };
}

//*****************************************************************
TStorage::TStorage(void): 
    m_nFlags(0),
    m_nTransCount(0), m_nWaitingTransCount(0), m_bFlush(false), m_bRequestFlush(false),
    m_hSemTerminate(NULL)
{
    for(int ctr = 0; ctr < 10; ctr++)
    {
        HANDLE hSem = CreateSemaphore(NULL, 0, 100, NULL);
        m_semaphorePool.push(hSem);
    }
    m_modificationKey = m_pageCache.getModificationKey();
}

//*****************************************************************
TStorage::~TStorage(void)
{
    while(!m_semaphorePool.empty())
    {
        HANDLE hSem = m_semaphorePool.top();
        m_semaphorePool.pop();
        CloseHandle(hSem);
    }
    if(m_hSemTerminate)
    {
        CloseHandle(m_hSemTerminate);
    }
}

//*****************************************************************
HANDLE TStorage::getSemaphoreFromPool()
{
    if(m_semaphorePool.empty())
    {
        for(int ctr = 0; ctr < 10; ctr++)
        {
            HANDLE hSem = CreateSemaphore(NULL, 0, 100, NULL);
            m_semaphorePool.push(hSem);
        }
    }
    HANDLE hSem = m_semaphorePool.top();
    m_semaphorePool.pop();
    return hSem;
}

//*****************************************************************
TStorage* TStorage::open(LPCSTR a_pszPath)
{
    if(strlen(a_pszPath) == 0)
        return new TMemoryStorage();

    if(stricmp(a_pszPath, "$TEMP") == 0)
        return new TTempStorage();

    TStorage *pStorage = new TStorage();

    TStorageHandlerTask *pHandlerTask = TStorageHandlerTask::Start(pStorage, a_pszPath, THREAD_PRIORITY_NORMAL);
    if(pHandlerTask == NULL)
    {
        delete pStorage;
        return NULL;
    }
    pStorage->m_pHandlerTask = pHandlerTask;
    return pStorage;
}

//*****************************************************************
void TStorage::close()
{
    EnterCriticalSection(&m_cs);

    m_hSemTerminate = getSemaphoreFromPool();

    if(m_nTransCount > 0)
    {
        int nCount = m_nTransCount;
        LeaveCriticalSection(&m_cs);
        for(int ctr = 0; ctr < nCount; ctr++)
        {
            WaitForSingleObject(m_hSemTerminate, INFINITE);
        }
        EnterCriticalSection(&m_cs);
    }

    typedef Actor::TMessage0<TStorageHandlerTask> TMsgTerminate;
    TMsgTerminate *pMsgTerm = new TMsgTerminate();
    pMsgTerm->Send(m_pHandlerTask, &TStorageHandlerTask::Terminate);

    LeaveCriticalSection(&m_cs);

    WaitForSingleObject(m_hSemTerminate, INFINITE);
    delete this;
}

//*****************************************************************
bool TStorage::getPageForRead(FPOINTER a_fpPage, DataPage::TPageRef& a_ref)
{
    if(m_pageCache.getPageForRead(a_fpPage, a_ref))
        return true;

    ensurePage(a_fpPage);
    if(!m_pageCache.getPageForRead(a_fpPage, a_ref))
    {
        throwPageException(a_fpPage);
    };
    return true;
}

//*****************************************************************
bool TStorage::getPageForWrite(FPOINTER a_fpPage, DataPage::TPageRef& a_ref)
{
    if(!m_pageCache.getPageForWrite(a_fpPage, a_ref))
    {
        ensurePage(a_fpPage);
        if(!m_pageCache.getPageForWrite(a_fpPage, a_ref))
        {
            throwPageException(a_fpPage);
        }
    }

    TCSLock _lock(&m_cs);
    FPOINTER fpPageTop = a_fpPage + 1;
    if(m_header.m_fpEnd < fpPageTop)
    {
        m_header.m_fpEnd = fpPageTop;
        m_header.m_bDirty = TRUE;
        m_fpTop = fpPageTop;
    }
    return true;
}

//*****************************************************************
void TStorage::ensurePage(FPOINTER a_fpPage)
{
    HANDLE hSem = NULL;
    //-------------------------------------------------------------------
    {
        TCSLock _lock(&m_cs);
        
        TPageRequestMapIter iter = m_mapPageRequest.find(a_fpPage);

        if(iter == m_mapPageRequest.end())
        {
            hSem = getSemaphoreFromPool();
            PAGE_REQUEST req;
            req.refCount = 1;
            req.hSem = hSem;
            m_mapPageRequest[a_fpPage] = req;

            typedef Actor::TMessage1<TStorageHandlerTask,FPOINTER> TMsgLoadPage;
            TMsgLoadPage *pMsgLoadPage = new TMsgLoadPage(a_fpPage);
            pMsgLoadPage->Send(m_pHandlerTask, &TStorageHandlerTask::LoadPage);
        }
        else
        {
            PAGE_REQUEST &refReq = iter->second;
            refReq.refCount++;
            hSem = refReq.hSem;
        }
    }
    //-------------------------------------------------------------------
    WaitForSingleObject(hSem, INFINITE);
    //-------------------------------------------------------------------
    {
        TCSLock _lock(&m_cs);

        TPageRequestMapIter iter = m_mapPageRequestUsed.find(a_fpPage);
        if(iter != m_mapPageRequestUsed.end())
        {
            PAGE_REQUEST &refReq = iter->second;
            if((--refReq.refCount) == 0)
            {
                m_semaphorePool.push(refReq.hSem);  // back to pool
                m_mapPageRequestUsed.erase(a_fpPage);
            }
        }
    }
}

//*****************************************************************
void TStorage::notifyPageLoaded(FPOINTER a_fpPage)
{
    TCSLock _lock(&m_cs);

    PAGE_REQUEST req = m_mapPageRequest[a_fpPage];
    m_mapPageRequestUsed[a_fpPage] = req;
    m_mapPageRequest.erase(a_fpPage);
    
    ReleaseSemaphore(req.hSem, req.refCount, NULL);
}

//*****************************************************************
void TStorage::throwPageException(FPOINTER a_fpPage)
{
    TPageFaultMapIter iter = m_mapPageFault.find(a_fpPage);
    if(iter != m_mapPageFault.end())
    {
        PAGE_FAULT& fault = iter->second;
        TStorageException::Throw(fault.type, fault.dwSysErrorCode);
    }
    else
    {
        TStorageException::Throw(PAGE_FAULT_NOT_EXISTS, 0);
    }
}

//*************************************************************
void TStorage::beginTrans(void)
{
    TCSLock _lock(&m_cs);

    if(m_bFlush)
    {
        m_nWaitingTransCount++;
        _lock.unlock();
        m_semTrans.down();
        _lock.lock();
    }
    m_nTransCount++;
}

//*************************************************************
void TStorage::endTrans(void)
{
    TCSLock _lock(&m_cs);

    if((--m_nTransCount) == 0)
    {
        if(m_bRequestFlush)          // Flush requested before
        {
            m_bRequestFlush = false;
            flush();
        }
        if(m_hSemTerminate != NULL)  // Termination in progress
        {
            ReleaseSemaphore(m_hSemTerminate, 1, NULL);
        }
    }
}

//*************************************************************
bool TStorage::beginFlush(void)
{
    TCSLock _lock(&m_cs);

    if(m_nTransCount > 0)
    {
        m_bRequestFlush = true;
        return false;
    }
    m_bFlush = true;
    return true;
}

//*************************************************************
void TStorage::endFlush(void)
{
    TCSLock _lock(&m_cs);

    m_bFlush = false;

    int nWaitingTransCount = m_nWaitingTransCount;
    if(nWaitingTransCount > 0)
    {
        m_nWaitingTransCount = 0;
        m_semTrans.up(nWaitingTransCount);
    }
}

//*************************************************************
void TStorage::flush()
{
    TCSLock _lock(&m_cs);

    if(m_modificationKey != m_pageCache.getModificationKey())
    {
        if(m_nTransCount > 0)
        {
            m_bRequestFlush = true;
            return;
        }
        m_bFlush = true;

        typedef Actor::TMessage0<TStorageHandlerTask> TMsgFlush;
        TMsgFlush *pMsgFlush = new TMsgFlush();
        pMsgFlush->Send(m_pHandlerTask, &TStorageHandlerTask::Flush);
    }
}

//*****************************************************************
void TStorage::setPageFault(FPOINTER a_fpPage, Storage::PAGE_FAULT &a_fault)
{
    TCSLock _lock(&m_cs);
    m_mapPageFault[a_fpPage] = a_fault;
    switch(a_fault.type)
    {
        case PAGE_FAULT_FILE:
            System::setAlarm("DATABASE", Util::message("%s OS error code %d, page %d", a_fault.szFileName, a_fault.dwSysErrorCode, a_fpPage));
            break;

        case PAGE_FAULT_CRC:
            System::setAlarm("DATABASE", Util::message("%s CRC error, page %d", a_fault.szFileName, a_fpPage));
            break;
    }
}

//*****************************************************************
void TStorage::getRootSecurity(Security::TSecurityAttributes &a_secAttrs)
{
    *a_secAttrs.getBase() = Security::Manager::AdminSecurity;
}

//*****************************************************************
TStorageHandlerTask* TStorageHandlerTask::Start(TStorage *a_pStorage, LPCSTR a_pszPath, int a_nPriority)
{
    TStorageHandlerTask *pTask = new TStorageHandlerTask();
    pTask->m_strPath = a_pszPath;
    pTask->m_pStorage = a_pStorage;

    if(pTask->TTask::Start(a_nPriority))
    {
        return pTask;
    }
    else
    {
        delete pTask;
        return NULL;
    }
}

//*************************************************************
BOOL TStorageHandlerTask::OnStart()
{
    Debug::TPageDataTrace::suppress();
    UpdateDeleteOnlyStatus();

    if(!m_tempFile.open())
        return FALSE;

    if(m_dataFile.Open(m_strPath))
    {
        m_dataFile.Recover();
        FILEHEADER *pHdr = m_dataFile.GetHeader();

        m_pStorage->setHeader(pHdr);
        m_pStorage->setTopPointer(pHdr->m_fpEnd);
        m_pStorage->m_modificationKey = m_pStorage->m_pageCache.getModificationKey();
        m_pStorage->m_bReadOnly = (m_dataFile.IsReadOnly() == TRUE);
        return TRUE;
    }

    return FALSE;
}

//*************************************************************
void TStorageHandlerTask::OnTimer(DWORD a_dwTicks)
{
    static int s_modKey = 0;

    // Detected modification out of timer
    if(m_dwFlushTime == 0 && m_pStorage->isModified())
    {
        s_modKey = m_pStorage->m_pageCache.getModificationKey();
        m_dwFlushTime = a_dwTicks + 5000;
    }
    // Flush timer started
    else if(m_dwFlushTime > 0)
    {
        // elapsed
        if(a_dwTicks > m_dwFlushTime)
        {
            Flush();
            return;
        }
        // update timer after changes
        int modKey = m_pStorage->m_pageCache.getModificationKey();
        if(modKey != s_modKey)
        {
            s_modKey = modKey;
            m_dwFlushTime = a_dwTicks + 5000;
        }
    }
}

//*************************************************************
bool TStorageHandlerTask::ReadSinglePage(FPOINTER a_fpPage, void *a_pvBuff, PAGE_FAULT *a_pFault)
{
    if(m_tempFile.readPage(a_fpPage, a_pvBuff))
        return true;

    try
    {
        m_dataFile.ReadPage(a_fpPage, a_pvBuff);
        return true;
    }
    catch(CFileException *e)
    {
        if(a_pFault)
        {
            a_pFault->type           = PAGE_FAULT_FILE;
            a_pFault->dwSysErrorCode = e->m_lOsError;
            strcpy(a_pFault->szFileName, m_strPath);
        }
        e->Delete();
    }
    catch(CUserException* e)
    {
        if(a_pFault)
        {
            a_pFault->type = PAGE_FAULT_CRC;
            strcpy(a_pFault->szFileName, m_strPath);
        }
        e->Delete();
    }
    return false;
}


//*************************************************************
void TStorageHandlerTask::LoadPage(FPOINTER a_fpPage)
{
    void *pvBuff = m_pStorage->m_pageCache.findDiscardedDirtyPage(a_fpPage);

    if(pvBuff != NULL)
    {
        DataPage::TPageRef refPage;
        m_pStorage->m_pageCache.getPageForWrite(a_fpPage, refPage, true);
        memcpy(refPage.getBuffer(), pvBuff, PAGESIZE);
        refPage.setDirty(false);
    }
    else if(!m_pStorage->m_pageCache.pageExists(a_fpPage))
    {
        BYTE       bufPage[PAGESIZE] = {0};
        PAGE_FAULT fault = {0};

        if(ReadSinglePage(a_fpPage, bufPage, &fault))
        {
            // Page exists - write to cache
            FPOINTER fpPage = a_fpPage;
            DataPage::TPageRef refPage;
            bool bLoaded = true;

            for(int ctr = 0; ctr < 10; ctr++)
            {
                if(!m_pStorage->m_pageCache.pageExists(fpPage))
                {
                    if(!bLoaded)
                    {
                        if(!ReadSinglePage(fpPage, bufPage, NULL))
                            break;
                    }
                    m_pStorage->m_pageCache.getPageForWrite(fpPage, refPage, true);
                    memcpy(refPage.getBuffer(), bufPage, PAGESIZE);
                    refPage.setDirty(false);
                }
                fpPage += 1;
                bLoaded = false;
            }
        }
        else if(a_fpPage >= m_pStorage->getTopPointer())
        {
            // New page
            FPOINTER fpPage = a_fpPage;
            DataPage::TPageRef refPage;

            for(int ctr = 0; ctr < 10; ctr++)
            {
                if(!m_pStorage->m_pageCache.pageExists(fpPage))
                {
                    m_pStorage->m_pageCache.getPageForWrite(fpPage, refPage, true);
                    memset(refPage.getBuffer(), 0, PAGESIZE);
                    refPage.setDirty(false);
                }
                fpPage += 1;
            }
        }
        else
        {
            m_pStorage->setPageFault(a_fpPage, fault);
        }
    }
    m_pStorage->notifyPageLoaded(a_fpPage);

    // Write discarded pages from cache to temporary file
    std::vector<DataPage::PageInfo> m_vecDiscardedPages;
    m_pStorage->m_pageCache.popDiscardedDirtyPages(m_vecDiscardedPages);

    typedef std::vector<DataPage::PageInfo>::iterator PageInfoIter;

    for(PageInfoIter iter = m_vecDiscardedPages.begin(); iter != m_vecDiscardedPages.end(); ++iter)
    {
        DataPage::PageInfo &info = *iter;
        m_tempFile.writePage(info);
        DataPage::TPagePool::getInstance().releaseBlock(info.second);
    }
    UpdateDeleteOnlyStatus();
}

//*************************************************************
void TStorageHandlerTask::UpdateDeleteOnlyStatus()
{
    ULARGE_INTEGER ulFreeSpace;

    GetDiskFreeSpaceEx(m_strPath, &ulFreeSpace, NULL, NULL);

    TCSLock _lock(&m_pStorage->m_cs);

    if((m_pStorage->m_nFlags & eFlag_DeleteOnly) == 0)
    {
        if(ulFreeSpace.QuadPart < 0x500000)  // 5M - high water
        {
            m_pStorage->m_nFlags |= eFlag_DeleteOnly;
        }
    }
    else
    {
        if(ulFreeSpace.QuadPart > 0x800000)  // 8M - low water
        {
            m_pStorage->m_nFlags &= ~eFlag_DeleteOnly;
        }
    }
}

//*************************************************************
void TStorageHandlerTask::Flush()
{
    if(!m_pStorage->beginFlush())
        return;

    FPOINTER fpEnd = m_pStorage->getTopPointer();
    m_dataFile.GrowFile(fpEnd);

    BOOL32 bNeedCompact = (m_pStorage->getFlags() & eFlag_NeedCompact) ? TRUE : FALSE;

    // Dirty pages from cache
    std::set<FPOINTER> setDirtyPages;
    m_pStorage->m_pageCache.getDirtyPages(setDirtyPages);

    //======================================================================
    {
        TCombinedPageIterator     iterator(setDirtyPages, &m_pStorage->m_pageCache, &m_tempFile);
        Util::TPageRangeIterator  rangeIterator(&iterator);

        m_dataFile.BeginWrite(&rangeIterator);
        {
            FILEHEADER hdr = m_pStorage->getHeader();
            if(hdr.m_bDirty)
            {
                *m_dataFile.GetHeader() = hdr;
                hdr.m_bDirty = false;
            }
            hdr.m_bNeedCompact = bNeedCompact;
            hdr.m_fpEnd = fpEnd;
            m_pStorage->setHeader(&hdr);
            *m_dataFile.GetHeader() = hdr;
            m_dataFile.WriteHeader();

            Util::TPageRange range;
            bool more = rangeIterator.getFirstRange(range);
            while(more)
            {
                m_dataFile.WritePages(range);
                more = rangeIterator.getNextRange(range);
            }
        }
        m_dataFile.EndWrite();

        //==========================================================================
        // Clear dirty flag
        DataPage::TPageRef refPage;

        typedef std::set<FPOINTER>::iterator PageSetIterator;
        for(PageSetIterator iter = setDirtyPages.begin(); iter != setDirtyPages.end(); ++iter)
        {
            m_pStorage->m_pageCache.getPageForRead(*iter, refPage, true);
            refPage.setDirty(false);
        }
    }
    //======================================================================
    m_tempFile.reset();
    m_pStorage->endFlush();

    TCSLock _lock(&m_pStorage->m_cs);
    m_pStorage->m_modificationKey = m_pStorage->m_pageCache.getModificationKey();
    m_dwFlushTime = 0;
    if(m_pStorage->m_nFlags & eFlag_EmptyPages)
    {
        m_pStorage->m_nFlags &= ~eFlag_EmptyPages;
        m_pStorage->m_nFlags |=  eFlag_NeedCompact;
    }
}

//*************************************************************
void TStorageHandlerTask::Terminate()
{
    if(m_pStorage->isModified())
    {
        Flush();
    }
    m_dataFile.Close();
    Stop();
    ReleaseSemaphore(m_pStorage->m_hSemTerminate, 1, NULL);
}

//*************************************************************
void TStorageException::Throw(int a_nType, DWORD a_nSysErrorCode)
{
    TStorageException* _this = &s_exception.Value();
    _this->m_nType = a_nType;
    _this->m_nSysErrorCode = a_nSysErrorCode;
    throw _this;
}

//*************************************************************
TMemoryStorage::TMemoryStorage(void)
{
    FILEHEADER hdr = {0};
    hdr.m_fpEnd = PAGES_OF_HEADER;
    hdr.m_nPageTabDirCount = 8;
    TStorage::setHeader(&hdr);
    TStorage::setTopPointer(PAGES_OF_HEADER);
    TStorage::m_bReadOnly = false;
}

//*************************************************************
void TMemoryStorage::ensurePage(FPOINTER a_fpPage)
{
    TCSLock _lock(&m_cs);

    // May be ensured by concurrent thread
    if(m_pageCache.pageExists(a_fpPage))
        return;

    DataPage::TPageRef ref;
    if(!m_pageCache.getPageForWrite(a_fpPage, ref, true))
        throwPageException(a_fpPage);
 
    TMapPagesIter it = m_mapPages.find(a_fpPage);
    if(it != m_mapPages.end()) // found
    {
        memcpy(ref.getBuffer(), it->second, PAGESIZE);
    }
    ref.setDirty(false);

    //  discarded pages
    typedef std::vector<DataPage::PageInfo>::iterator VecPageIter;

    std::vector<DataPage::PageInfo> m_vecDiscardedPages;

    m_pageCache.popDiscardedDirtyPages(m_vecDiscardedPages);

    for(VecPageIter it = m_vecDiscardedPages.begin(); it != m_vecDiscardedPages.end(); ++it)
    {
        FPOINTER fpPage = it->first;
        if(m_mapPages.find(fpPage) != m_mapPages.end())
        {
            memcpy(m_mapPages[fpPage], it->second, PAGESIZE);
        }
        else
        {
            m_mapPages[fpPage] = it->second;
        }
    }
}

//*************************************************************
void TMemoryStorage::close()
{
    TCSLock _lock(&m_cs);

    for(TMapPagesIter it = m_mapPages.begin(); it != m_mapPages.end(); it++)
    {
        DataPage::TPagePool::getInstance().releaseBlock(it->second);
    }
}

//*****************************************************************
void TMemoryStorage::getRootSecurity(Security::TSecurityAttributes &a_secAttrs)
{
    *a_secAttrs.getBase() = Security::Manager::SystemSecurity;
}

//*************************************************************
TTempStorage::TTempStorage(void): m_fpFileTop(0)
{
    FILEHEADER hdr = {0};
    hdr.m_fpEnd = PAGES_OF_HEADER;
    hdr.m_nPageTabDirCount = 16;
    TStorage::setHeader(&hdr);
    TStorage::setTopPointer(PAGES_OF_HEADER);
    TStorage::m_bReadOnly = false;

    m_file.open();
}

//*************************************************************
void TTempStorage::ensurePage(FPOINTER a_fpPage)
{
    TCSLock _lock(&m_cs);

    void *pvBuff = m_pageCache.findDiscardedDirtyPage(a_fpPage);

    if(pvBuff != NULL)
    {
        DataPage::TPageRef refPage;
        m_pageCache.getPageForWrite(a_fpPage, refPage, true);
        memcpy(refPage.getBuffer(), pvBuff, PAGESIZE);
        refPage.setDirty(false);
    }

    // May be ensured by concurrent thread
    if(m_pageCache.pageExists(a_fpPage))
        return;

    FILEHEADER header = getHeader();
    DataPage::TPageRef ref;

    for(FPOINTER fpPage = a_fpPage; fpPage < a_fpPage + 10; ++fpPage)
    {
        if(m_pageCache.pageExists(a_fpPage))
            continue;

        if(fpPage < m_fpFileTop)
        {
            void* pvPage = m_file.ptrFromFileOffset(OFFSET_FT(fpPage) * PAGESIZE);
            m_pageCache.getPageForWrite(fpPage, ref, true);
            memcpy(ref.getBuffer(), pvPage, PAGESIZE);
        }
        else
        {
            m_pageCache.getPageForWrite(fpPage, ref, true);
            memset(ref.getBuffer(), 0, PAGESIZE);
        }
        ref.setDirty(false);
    }

    // Write discarded pages from cache to temporary file
    std::vector<DataPage::PageInfo> m_vecDiscardedPages;
    m_pageCache.popDiscardedDirtyPages(m_vecDiscardedPages);

    typedef std::vector<DataPage::PageInfo>::iterator PageInfoIter;

    for(PageInfoIter iter = m_vecDiscardedPages.begin(); iter != m_vecDiscardedPages.end(); ++iter)
    {
        DataPage::PageInfo &info = *iter;

        FPOINTER fpPage = info.first;
        if(fpPage >= m_fpFileTop)
            m_fpFileTop = fpPage + 1;

        void* pvPage = m_file.ptrFromFileOffset(OFFSET_FT(fpPage) * PAGESIZE);
        memcpy(pvPage, info.second, PAGESIZE);
        DataPage::TPagePool::getInstance().releaseBlock(info.second);
    }

}

//*************************************************************
void TTempStorage::getRootSecurity(Security::TSecurityAttributes &a_secAttrs)
{
    *a_secAttrs.getBase() = Security::Manager::TempSecurity;
}