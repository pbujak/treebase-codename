#pragma once

class TPageSequence
{
    BYTE*       m_byBuff;
    SYSTEM_INFO m_sysInfo;

public:
    TPageSequence(void);
public:
    ~TPageSequence(void);

    void* getPageForRead(FPOINTER a_fpPage);
    void* getPageForWrite(FPOINTER a_fpPage);
};
