#include "StdAfx.h"
#include "MockStorage.h"
#include "MockDBManager.h"
#include "..\..\SegmentMgr.h"

MockDBManager G_dbManager;

static TSegment m_pageDir;

MockDBManager::MockDBManager(void)
{
    m_entry.m_pSegmentDir = &m_pageDir;
    RWLock_Initialize(&m_entry.m_rwLock);
}

MockDBManager::~MockDBManager(void)
{
    RWLock_Delete(&m_entry.m_rwLock);
}

Storage::TStorage* MockDBManager::GetFile(long a_nID)
{
    return TASK_GetFixedStorage();
}

TDBFileEntry* MockDBManager::operator[](long a_nID)
{
    m_entry.m_pStorage = TASK_GetFixedStorage();
    return &m_entry;
};
