#include "StdAfx.h"
#include "MockTask.h"
#include "..\..\Util.h"

#pragma comment(lib, "Rpcrt4.lib")

static TTask s_task;

DWORD TTask::S_dwCount = 1;


TTask::TTask(void)
{
}

TTask::~TTask(void)
{
}

LPVOID TASK_MemDup(LPVOID a_pvData)
{
    if(a_pvData == NULL)
        return NULL;

    int cbSize = TASK_MemSize(a_pvData);
    LPVOID pvData2 = TASK_MemAlloc(cbSize);
    memcpy(pvData2, a_pvData, cbSize);
    return pvData2;
}

TTask* TASK_GetCurrent()
{
    return &s_task;
}

void TASK_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem, LPCSTR a_pszErrorSection)
{
    s_task.m_dwError = a_dwError;
    s_task.m_strErrorItem = a_pszErrorItem;
    s_task.m_strErrorSection = a_pszErrorSection;
}

//************************************************************************
LPVOID TTaskMemAlloc::Alloc(DWORD a_cbSize)
{
    return malloc(a_cbSize);
};

//************************************************************************
void TTaskMemAlloc::Free(LPVOID a_pvData)
{
    free(a_pvData);
};

//************************************************************************
LPVOID TTaskMemAlloc::ReAlloc(LPVOID a_pvData, DWORD a_cbSize)
{
    return realloc(a_pvData, a_cbSize);
};

//************************************************************************
DWORD TTaskMemAlloc::MemSize(LPVOID a_pvData)
{
    return _msize(a_pvData);
};

//************************************************************************
template<>
bool Util::GetEnvironmentString<bool>(const char *a_pszEnvVarName)
{
    return false;
}

static const char* UUID_STRING_FORMAT = "%8.8X-%4.4X-%4.4X-%4.4X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X";

Util::Uuid::Uuid()
{
    memset(this, 0, sizeof(Uuid));
}

CString Util::UuidToString(const Util::Uuid &_uuid)
{
    CString result;

    char *buff = result.GetBuffer(40);
    sprintf(buff, UUID_STRING_FORMAT, 
        _uuid.data1, _uuid.data2, _uuid.data3, _uuid.data4,
        _uuid.data5[0], _uuid.data5[1], _uuid.data5[2],
        _uuid.data5[3], _uuid.data5[4], _uuid.data5[5]
    );

    result.ReleaseBuffer();

    return result;
}

Util::Uuid Util::UuidFromString(const CString &_string)
{
    Uuid uuid;

    sscanf((LPCSTR)_string, UUID_STRING_FORMAT, 
        &uuid.data1, &uuid.data2, &uuid.data3, &uuid.data4,
        &uuid.data5[0], &uuid.data5[1], &uuid.data5[2],
        &uuid.data5[3], &uuid.data5[4], &uuid.data5[5]
    );

    return uuid;
}

Util::Uuid Util::UuidGenerate()
{
    GUID winGUID;

    ::UuidCreate(&winGUID);

    Uuid uuid;
    uuid.data1 = winGUID.Data1;
    uuid.data2 = winGUID.Data2;
    uuid.data3 = winGUID.Data3;
    uuid.data4 = *reinterpret_cast<WORD*>(&winGUID.Data4[0]);
    memcpy(&uuid.data5, &winGUID.Data4[2], 6);
    return uuid;
}

bool operator==(const Util::Uuid &uid1, const Util::Uuid &uid2)
{
    return memcmp(&uid1, &uid2, sizeof(Util::Uuid)) == 0;
}

Util::Uuid Util::EmptyUuid;