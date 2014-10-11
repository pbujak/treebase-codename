#pragma once
#include "..\..\syncobj.h"

class TSegment;

struct TDBFileEntry
{
    long               m_ndbID; 
    CString            m_strFile; 
    CString            m_strName;
    Storage::TStorage *m_pStorage;
    TSegment            *m_pSegmentDir; 
    long               m_nRefs; 
    RWLOCK             m_rwLock;
    BOOL               m_bCheckMedia;
    DWORD              m_dwLastAccessTime;
};


class MockDBManager
{
    TDBFileEntry m_entry;
public:
    MockDBManager(void);
public:
    ~MockDBManager(void);

public:
	Storage::TStorage* GetFile (long a_nID);
    TDBFileEntry* operator[](long a_nID);

    inline long GetCount()
    {
        return 1;
    }
};

extern MockDBManager G_dbManager;

