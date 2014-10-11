#pragma once
#include "TValueMap.h"
#include <afxcoll.h>
#include "syncobj.h"
#include "util.h"
#include "TTask.h"

namespace System
{
    class TValueEntry
    {
    public:
        DWORD          m_dwType;
        TBVALUE_ENTRY *m_pEntry;
    public:
        TValueEntry(long a_nValue);
        TValueEntry(LPCSTR a_pszValue);
        TValueEntry(const CString &a_strValue);
        ~TValueEntry();
    };

    class TStatusMap: public TValueMap
    {
        CMapStringToPtr m_mapKeys;
        BOOL            m_bModified;
    public:
        TStatusMap();

        inline BOOL& IsModified(){ return m_bModified; }

        BOOL SetValue(LPCSTR a_pszName, const TValueEntry &a_entry);
    	BOOL RemoveValue(LPCSTR a_pszName);
        inline LPVOID ExportBatch()
        {
            LPVOID pvBatch = TValueMap::ExportBatch();
            TValueMap::Clear();
            return pvBatch;
        }
    };
    
    class TSystemStatus;
    typedef TStatusMap (TSystemStatus::*PTR_STATUS_MAP);

    typedef struct _STATUS_MAP_ENTRY
    {
        LPCSTR         pszSection;
        PTR_STATUS_MAP pStatusMap;
    }STATUS_MAP_ENTRY;

    class TSystemStatusHandlerTask;

    class TSystemStatus
    {
        friend class TSystemStatusHandlerTask;

        static STATUS_MAP_ENTRY s_members[];
        static TSystemStatus    s_instance;

        TStatusMap       m_alarms, m_sessions, m_databases;
        THandle          m_hEvent;
        TCriticalSection m_cs;        
        int              m_nModifyCount;
        TSystemStatusHandlerTask *m_pHandlerTask;

        void HandleModifyFlag(TStatusMap *pMap);

        TSystemStatus(void);
    public:
        enum EType {eALARM = 0, eSESSION, eDATABASE, eTypeMax};

    public:
        static TSystemStatus& GetInstance()
        {
            return s_instance;
        }

        BOOL SetValue(EType a_eType, LPCSTR a_pszName, const TValueEntry &a_entry);
        BOOL RemoveValue(TSystemStatus::EType a_eType, LPCSTR a_pszName);
        void StartHandler();
        void StopHandler();
        inline int GetNextID(EType a_eID)
        {
            TCSLock _lock(&m_cs);
            return ++m_nNextID[a_eID];
        }
    public:
        ~TSystemStatus(void);

    public:
        int  m_nNextID[eTypeMax];
    };

    class TSystemStatusHandlerTask: public TTask
    {
        friend class TSystemStatus;

        BOOL           m_bStop;
        TSystemStatus *m_pStatus;
    public:
        inline TSystemStatusHandlerTask(TSystemStatus* a_pStatus): 
            m_pStatus(a_pStatus), 
            m_bStop(FALSE)
        {
        }
        inline BOOL IsStatusModified(TStatusMap *m_pStatusMap)
        {
            TCSLock _lock(&m_pStatus->m_cs);
            return m_pStatusMap->IsModified();
        }

        virtual void OnRun();
    };

    CString addSession(LPCSTR a_pszSession);
    void removeSession(LPCSTR a_pszSessionKey);
    CString addDatabase(LPCSTR a_pszDB);
    void removeDatabase(LPCSTR a_pszDBKey);

    void setAlarm(LPCSTR a_pszCategory, LPCSTR a_pszText);
} // namespace System