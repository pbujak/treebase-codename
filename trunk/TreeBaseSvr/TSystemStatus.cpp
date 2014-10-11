#include "stdafx.h"
#include "TSystemStatus.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TSegmentPageIndexMap.h"
#include "TBaseFile.h"
#include "section.h"

extern TThreadLocal<TSegmentPageIndexMap> G_segPageIndexMap;
extern TAppMemAlloc                     G_appMalloc;


//**************************************************************************
CString System::addDatabase(LPCSTR a_pszDBFile)
{
    CString strKey;
    CString strDB(a_pszDBFile);
    strKey.Format("DB%d", TSystemStatus::GetInstance().GetNextID(TSystemStatus::eDATABASE));

    if(strDB.IsEmpty())
        strDB = ":memory";

    TSystemStatus::GetInstance().SetValue(TSystemStatus::eDATABASE, strKey, (LPCSTR)strDB);
    return strKey;
}

//**************************************************************************
void System::removeDatabase(LPCSTR a_pszDBKey)
{
    TSystemStatus::GetInstance().RemoveValue(TSystemStatus::eDATABASE, a_pszDBKey);
}

//**************************************************************************
CString System::addSession(LPCSTR a_pszSession)
{
    CString strKey;
    strKey.Format("Client %d", TSystemStatus::GetInstance().GetNextID(TSystemStatus::eSESSION));

    TSystemStatus::GetInstance().SetValue(TSystemStatus::eSESSION, strKey, a_pszSession);
    return strKey;
}


//**************************************************************************
void System::removeSession(LPCSTR a_pszSessionKey)
{
    TSystemStatus::GetInstance().RemoveValue(TSystemStatus::eSESSION, a_pszSessionKey);
}

//**************************************************************************
void System::setAlarm(LPCSTR a_pszCategory, LPCSTR a_pszText)
{
    CTime time = CTime::GetCurrentTime();

    CString strKey;
    strKey.Format("A%d:%d-%d-%d %d:%d [%s]",
        TSystemStatus::GetInstance().GetNextID(TSystemStatus::eALARM),
        time.GetYear(), time.GetMonth(), time.GetDay(),
        time.GetHour(), time.GetMinute(),
        a_pszCategory);

    TSystemStatus::GetInstance().SetValue(TSystemStatus::eALARM, strKey, a_pszText);
}

using namespace System;

//**************************************************************************
TValueEntry::TValueEntry(long a_nValue)
{
    m_pEntry = (TBVALUE_ENTRY*)G_appMalloc.Alloc(sizeof(TBVALUE_ENTRY));
    if(m_pEntry)
    {
        m_pEntry->int32 = a_nValue;
        m_dwType = TBVTYPE_INTEGER;
    }
}

//**************************************************************************
TValueEntry::TValueEntry(LPCSTR a_pszValue)
{
    m_pEntry = (TBVALUE_ENTRY*)G_appMalloc.Alloc(sizeof(TBVALUE_ENTRY) + strlen(a_pszValue));
    if(m_pEntry)
    {
        strcpy(m_pEntry->text, a_pszValue);
        m_dwType = TBVTYPE_TEXT;
    }
}

//**************************************************************************
TValueEntry::TValueEntry(const CString &a_strValue)
{
    m_pEntry = (TBVALUE_ENTRY*)G_appMalloc.Alloc(sizeof(TBVALUE_ENTRY) + a_strValue.GetLength());
    if(m_pEntry)
    {
        strcpy(m_pEntry->text, a_strValue);
        m_dwType = TBVTYPE_TEXT;
    }
}

//**************************************************************************
TValueEntry::~TValueEntry()
{
    if(m_pEntry)
        G_appMalloc.Free(m_pEntry);
}

//**************************************************************************
TStatusMap::TStatusMap(): TValueMap(&G_appMalloc), m_bModified(FALSE)
{
}

//**************************************************************************
BOOL TStatusMap::SetValue(LPCSTR a_pszName, const TValueEntry &a_entry)
{
    if(!TValueMap::SetValue(a_pszName, a_entry.m_dwType, a_entry.m_pEntry))
        return FALSE;

    m_mapKeys.SetAt(a_pszName, (void*)a_pszName);
    return TRUE;
}

//**************************************************************************
BOOL TStatusMap::RemoveValue(LPCSTR a_pszName)
{
    void* ptr = NULL;

    if(m_mapKeys.Lookup(a_pszName, ptr))
    {
        if(TValueMap::FindValue(a_pszName))
        {
            TValueMap::RemoveValue(a_pszName);
        }
        else
            TValueMap::SetValueDeleted(a_pszName);
        m_mapKeys.RemoveKey(a_pszName);
        return TRUE;
    }
    return FALSE;
}

STATUS_MAP_ENTRY TSystemStatus::s_members[] = {
    {"\\System\\Status\\Alarms",    &TSystemStatus::m_alarms},
    {"\\System\\Status\\Sessions",  &TSystemStatus::m_sessions},
    {"\\System\\Status\\Databases", &TSystemStatus::m_databases}
};

TSystemStatus TSystemStatus::s_instance;

//**************************************************************************
TSystemStatus::TSystemStatus(void): 
    m_nModifyCount(0), 
    m_hEvent(CreateEvent(NULL, TRUE, FALSE, NULL)),
    m_pHandlerTask(NULL)
{
    memset(m_nNextID, 0, sizeof(m_nNextID));
}

//**************************************************************************
TSystemStatus::~TSystemStatus(void)
{
}

//**************************************************************************
void TSystemStatus::HandleModifyFlag(TStatusMap *a_pMap)
{
    if(!a_pMap->IsModified())
    {
        if((m_nModifyCount++) == 0)
            SetEvent(m_hEvent);
    }
    a_pMap->IsModified() = TRUE;
}

//**************************************************************************
BOOL TSystemStatus::SetValue(TSystemStatus::EType a_eType, LPCSTR a_pszName, const TValueEntry &a_entry)
{
    TCSLock _lock(&m_cs);

    TStatusMap *pMap = &(this->*s_members[a_eType].pStatusMap);

    if(!pMap->SetValue(a_pszName, a_entry))
        return FALSE;
    
    HandleModifyFlag(pMap);
    return TRUE;
}

//**************************************************************************
BOOL TSystemStatus::RemoveValue(TSystemStatus::EType a_eType, LPCSTR a_pszName)
{
    TCSLock _lock(&m_cs);

    TStatusMap *pMap = &(this->*s_members[a_eType].pStatusMap);

    if(!pMap->RemoveValue(a_pszName))
        return FALSE;
    
    HandleModifyFlag(pMap);
    return TRUE;
}

//**************************************************************************
void TSystemStatusHandlerTask::OnRun()
{
    while(true)
    {
        WaitForSingleObject(m_pStatus->m_hEvent, INFINITE);

        if(m_bStop)
            return;
        
        Util::TState<TDBManager> _lockShared(&G_dbManager, &TDBManager::LockShared, &TDBManager::UnlockShared);

        for(int i = 0; i < TSystemStatus::eTypeMax; i++)
        {
            TStatusMap *pMap = &(m_pStatus->*TSystemStatus::s_members[i].pStatusMap);
            
            TCSLock _lock(&m_pStatus->m_cs);
            if(pMap->IsModified())
            {
                _lock.unlock();
                Util::TSectionPtr pSection = SECTION_OpenSection(GetRootSection(), TSystemStatus::s_members[i].pszSection, TBOPEN_MODE_DYNAMIC);
                BOOL bLockSection = pSection ? pSection->Lock() : FALSE;
                _lock.lock();
                if(bLockSection)
                {
                    LPVOID pvBatch = pMap->ExportBatch();
                    _lock.unlock();
                    pSection->ProcessBatch(pvBatch);
                    _lock.lock();
                    pMap->IsModified() = FALSE;
                    m_pStatus->m_nModifyCount--;
                    TASK_MemFree(pvBatch);
                }
                if(pSection)
                    pSection->Unlock();
            }
        }

        Storage::TAdvPageRef xPageRef(0);
        xPageRef.releaseAll();
        G_segPageIndexMap.Value().reset();

        TCSLock _lock(&m_pStatus->m_cs);
        if(m_pStatus->m_nModifyCount == 0)
            ResetEvent(m_pStatus->m_hEvent);
    }
}

//**************************************************************************
void TSystemStatus::StartHandler()
{
    if(m_pHandlerTask)
        return;

    m_pHandlerTask = new TSystemStatusHandlerTask(this);
    if(!m_pHandlerTask->Start(THREAD_PRIORITY_BELOW_NORMAL, 0x10000, 0x100000))
    {
        _release_ptr(m_pHandlerTask);
        return;
    }
}

//**************************************************************************
void TSystemStatus::StopHandler()
{
    m_pHandlerTask->m_bStop = TRUE;
    SetEvent(m_hEvent);
}

