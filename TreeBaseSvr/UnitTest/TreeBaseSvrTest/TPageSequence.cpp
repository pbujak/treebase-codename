#include "StdAfx.h"
#include "TPageSequence.h"

TPageSequence::TPageSequence(void)
{
    m_byBuff = (BYTE *)VirtualAlloc(NULL, 300000000, MEM_RESERVE, PAGE_READWRITE);
    ASSERT(m_byBuff != NULL);
    memset(&m_sysInfo, 0, sizeof(m_sysInfo));
    GetSystemInfo(&m_sysInfo);
}

TPageSequence::~TPageSequence(void)
{
    VERIFY( VirtualFree(m_byBuff, 300000000, MEM_DECOMMIT) );
    VERIFY( VirtualFree(m_byBuff, 0, MEM_RELEASE) );
    DWORD dwErr = GetLastError();
}

void* TPageSequence::getPageForRead(FPOINTER a_fpPage)
{
    int offset = m_sysInfo.dwPageSize * a_fpPage;

    MEMORY_BASIC_INFORMATION memInfo = {0};
    VirtualQuery(&m_byBuff[offset], &memInfo, sizeof(&memInfo));

    if(memInfo.State != MEM_COMMIT)
    {
        VirtualAlloc(&m_byBuff[offset], m_sysInfo.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    }
    DWORD dwOldProtect = 0;
    VirtualProtect(&m_byBuff[offset], m_sysInfo.dwPageSize, PAGE_READONLY, &dwOldProtect);
    return &m_byBuff[offset];
}

void* TPageSequence::getPageForWrite(FPOINTER a_fpPage)
{
    int offset = m_sysInfo.dwPageSize * a_fpPage;

    MEMORY_BASIC_INFORMATION memInfo = {0};
    VirtualQuery(&m_byBuff[offset], &memInfo, sizeof(&memInfo));

    if(memInfo.State != MEM_COMMIT)
    {
        VirtualAlloc(&m_byBuff[offset], m_sysInfo.dwPageSize, MEM_COMMIT, PAGE_READWRITE);
    }
    DWORD dwOldProtect = 0;
    VirtualProtect(&m_byBuff[offset], m_sysInfo.dwPageSize, PAGE_READWRITE, &dwOldProtect);
    return &m_byBuff[offset];
}
