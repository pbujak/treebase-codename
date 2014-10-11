#include "stdafx.h"
#include "TDataFile.h"
#include "Storage.h"
#include <afxtempl.h>
#include <afxwin.h>
#include <shlwapi.h>
#include "TSystemStatus.h"
#include "Util.h"
#include "PageRange.h"

using namespace Storage;

#define CTLCODE   0xFFDD0087
#define CRC32POLY 0xedb88320L

typedef struct _CTL_RANGE_COPY
{
    DATAFILEPOS dfpOffset;
    DWORD       dwSize;
}CTL_RANGE_COPY;


SEGID_F  SEGID_NULL = {0,0};

//===========================================================
template<> UINT AFXAPI HashKey<SEGID_F&>(SEGID_F& a_rSegIdf)
{
    return UINT(a_rSegIdf.ndbId + a_rSegIdf.nSegId);
};

//===========================================================
static DATAFILEPOS _PAGESIZE_2_PAGESIZE_CRC_(FPOINTER a_fpSize)
{
    DATAFILEPOS dfpSize = LONGLONG(a_fpSize) * PAGESIZE_CRC;
    return dfpSize;
}

//*************************************************************************
TDataFile::TDataFile(void)
{
    memset(&m_pageChunk, 0, sizeof(m_pageChunk));
    memset(&m_emptyPage, 0, sizeof(m_emptyPage));
}

//*************************************************************************
TDataFile::~TDataFile(void)
{
}

//*************************************************************************
BOOL TDataFile::BeginWrite(Util::TPageRangeIterator *a_pIterator)
{
    FPOINTER       fpBase = 0; 
    FILEHEADER     header = {0};
    CTL_RANGE_COPY xData  = {0};  
    int            nCode = 0;
    int            nSize = 0;
    DWORD          dwCRC = 0;  
    Util::TComputeCRC   computeCRC;

    try
    {
        m_fileControl.SetLength(0);
        m_file.SeekToBegin();
        m_file.Read(&header, sizeof(header));
        m_file.Read(&dwCRC,  sizeof(dwCRC));
        m_fileControl.Write(&nCode,  sizeof(nCode));
        m_fileControl.Write(&header, sizeof(header));
        m_fileControl.Write(&dwCRC,  sizeof(dwCRC));
        FPOINTER fpSizePos = (FPOINTER)m_fileControl.GetPosition();
        m_fileControl.Write(&nSize,  sizeof(nSize));
    
        Util::TPageRange range;

        bool more = a_pIterator->getFirstRange(range);
        while (more)
        {
            m_pageChunk.fpFirst = range.getFirstPage();
            m_pageChunk.count   = range.getCount();

            if(fpBase == FPOINTER(-1))
                AfxThrowFileException(CFileException::invalidFile);

            xData.dfpOffset = DATAFILEPOS(m_pageChunk.fpFirst) * PAGESIZE_CRC;
            xData.dwSize    = DWORD(m_pageChunk.count) * PAGESIZE_CRC;

            m_file.Seek(xData.dfpOffset, CFile::begin);
            m_file.Read(m_pageChunk.entries, xData.dwSize);

            m_fileControl.Write(&xData, sizeof(xData));
            m_fileControl.Write(m_pageChunk.entries, xData.dwSize);

            more = a_pIterator->getNextRange(range);
            nSize++;
        }
        m_fileControl.Seek(fpSizePos, CFile::begin);
        m_fileControl.Write(&nSize,  sizeof(nSize));
        m_fileControl.Flush();
        nCode = CTLCODE;
        m_fileControl.SeekToBegin();
        m_fileControl.Write(&nCode,  sizeof(nCode));
        m_fileControl.Flush();
    }
    catch(CFileException *e)
    {
        char szMsgBuff[300] = {0};
        e->GetErrorMessage(szMsgBuff, sizeof(szMsgBuff));
        System::setAlarm("DATABASE", szMsgBuff);
        e->Delete();
        return FALSE;
    }

    return TRUE;
}

//*************************************************************************
void TDataFile::EndWrite()
{
    try
    {
        DATAFILEPOS dfpEnd = _PAGESIZE_2_PAGESIZE_CRC_(m_header.m_fpEnd); 
        m_file.SetLength(dfpEnd);
        m_file.Flush();
        m_fileControl.SetLength(0);
        m_fileControl.Flush();
    }
    catch(CFileException *)
    {
    }
}

//*************************************************************************
BOOL TDataFile::GrowFile(DWORD a_dwNewSize)
{
    DATAFILEPOS dfpNewSize = _PAGESIZE_2_PAGESIZE_CRC_(a_dwNewSize);
    DATAFILEPOS dfpSize    = _PAGESIZE_2_PAGESIZE_CRC_(m_header.m_fpEnd);

    if (dfpNewSize < dfpSize)
        return TRUE;

    PAGE_CHUNK_ENTRY *pEmptyPage = GetEmptyPageBuffer();
    try
    {
        m_file.SetLength(dfpNewSize);
        m_file.Seek(dfpSize, CFile::begin);
        while (dfpSize < dfpNewSize)
        {
            m_file.Write(pEmptyPage, sizeof(PAGE_CHUNK_ENTRY));
            dfpSize += PAGESIZE_CRC;
        }
    }
    catch (CFileException*)
    {
        m_file.SetLength(dfpSize);
        return FALSE;
    }
    return TRUE;
}

//*************************************************************************
BOOL TDataFile::Open(LPCSTR a_pszFileName)
{
    Util::TComputeCRC computeCRC;
    char szCtlFileName[MAX_PATH+1] = {0};
    DWORD dwCRC=0, dwCRC1=0;

    m_bReadOnly = FALSE;
    strncpy(szCtlFileName, a_pszFileName, MAX_PATH);

    PathRenameExtension(szCtlFileName, ".ctl");

    memset(&m_header, 0, sizeof(FILEHEADER));

    if (!m_fileControl.Open(szCtlFileName, CFile::modeCreate|CFile::modeNoTruncate|CFile::modeReadWrite))
    {
        m_bReadOnly = TRUE;
    };

    if(m_bReadOnly)
    {
        if (!m_file.Open(a_pszFileName, CFile::modeNoTruncate|CFile::modeRead))
            return FALSE;
    }
    else
    {
        if (!m_file.Open(a_pszFileName, CFile::modeCreate|CFile::modeNoTruncate|CFile::modeReadWrite))
        {
            if(!m_file.Open(a_pszFileName, CFile::modeNoTruncate|CFile::modeRead))
            {
                m_fileControl.Close();
                return FALSE;
            }
            m_bReadOnly = TRUE;
        }
    }

    if (m_file.GetLength() > 0)
    {
        try{
            m_file.Seek(0, CFile::begin);
            m_file.Read(&m_header, sizeof(FILEHEADER));
            m_file.Read(&dwCRC,    sizeof(dwCRC));
            dwCRC1 = computeCRC.crc(&m_header, sizeof(FILEHEADER));
            if (dwCRC1!=dwCRC)
            {
                if (m_fileControl.GetLength()>0)
                {
                    System::setAlarm("DATABASE", Util::message("%s: Open failed CRC error", m_file.GetFileName()));
                    return FALSE;
                }
            }
        }
        catch(CException *)
        {
            m_file.Close();
            return FALSE;
        }
        return TRUE;
    }
    else
    {
        memset(&m_header, 0, sizeof(FILEHEADER));
        m_header.m_fpEnd = PAGES_OF_HEADER; //not PAGESIZE_CRC
        m_header.m_bDirty = TRUE;
        InitKey();

        m_header.m_nPageTabDirCount = 16;

        DATAFILEPOS dfpOffset = DATAFILEPOS(m_header.m_fpEnd) * PAGESIZE_CRC;
        m_file.Seek(dfpOffset, CFile::begin);

        PAGE_CHUNK_ENTRY *pEmptyPage = GetEmptyPageBuffer();

        for(int ctr = 0; ctr < m_header.m_nPageTabDirCount; ctr++)
        {
            m_file.Write(pEmptyPage, sizeof(PAGE_CHUNK_ENTRY));
            m_header.m_fpEnd++;
        }

        return WriteHeader();
    }
    return FALSE;
}

//*************************************************************************
void TDataFile::InitKey()
{
    SYSTEMTIME sysTime  = {0};
    BYTE      *pSysTime = (BYTE*)&sysTime;
    long       nCount   = 0;
    long       ctr      = 0;

    GetSystemTime(&sysTime);
    srand(sysTime.wMilliseconds);
    nCount = rand() % 64 + 64;

    for (ctr=0; ctr<nCount; ctr++)
    {
        m_header.m_byKey[ctr] = pSysTime[ctr % sizeof(SYSTEMTIME)];
    }
    m_header.m_byKeySize = nCount;
}

//*************************************************************************
void TDataFile::Close()
{
    m_file.Close();
	if(m_fileControl.m_hFile != INVALID_HANDLE_VALUE)
    {
        CString strName = m_fileControl.GetFilePath();
		m_fileControl.Close();
        remove(strName);
    }
}

//*************************************************************************
BOOL TDataFile::WriteHeader()
{
    Util::TComputeCRC computeCRC;
    DWORD       dwCRC = 0;
    try{
        m_file.Seek(0, CFile::begin);
        m_file.Write(&m_header, sizeof(FILEHEADER));
        dwCRC = computeCRC.crc(&m_header, sizeof(FILEHEADER));
        m_file.Write(&dwCRC, sizeof(dwCRC));
    }
    catch(CException *)
    {
        m_file.Close();
        return FALSE;
    }
    return TRUE;
}

//*************************************************************************
void TDataFile::Recover()
{
    FPOINTER       fpBase = 0; 
    FILEHEADER     header = {0};
    CTL_RANGE_COPY xData  = {0};  
    long           nSize = 0;
    long           nPage=0;
    long           nCode=0;
    DWORD          dwCRC=0, dwCRC1=0;
    Util::TComputeCRC   computeCRC;

    if (m_bReadOnly || m_fileControl.GetLength()==0)
        return;

    try
    {
        m_fileControl.SeekToBegin();
        m_fileControl.Read(&nCode,  sizeof(nCode));
        if (nCode != CTLCODE)
            return;
        m_fileControl.Read(&header, sizeof(header));
        m_fileControl.Read(&dwCRC,  sizeof(dwCRC));
        m_file.SeekToBegin();
        m_file.Write(&header, sizeof(header));
        m_file.Write(&dwCRC,  sizeof(dwCRC));

        m_fileControl.Read(&nSize, sizeof(nSize));

        for (int i = 0; i < nSize; i++)
        {
            m_fileControl.Read(&xData, sizeof(xData));
            m_fileControl.Read(m_pageChunk.entries, xData.dwSize);

            m_file.Seek (xData.dfpOffset, CFile::begin);
            m_file.Write(m_pageChunk.entries, xData.dwSize);
        }
        m_file.SetLength(DATAFILEPOS(header.m_fpEnd) * PAGESIZE_CRC);
        m_file.Flush();
        m_fileControl.SetLength(0);
        m_fileControl.Flush();
    }
    catch(CException*)
    {
    }
}

//*************************************************************************
BOOL TDataFile::WritePages(Util::TPageRange &a_range)
{
    m_pageChunk.fpFirst = a_range.getFirstPage();
    m_pageChunk.count   = a_range.getCount();

    Util::TComputeCRC computeCRC;
    long nKeySize = long(m_header.m_byKeySize); 

    PAGE_CHUNK_ENTRY* pEntry = m_pageChunk.entries;

    for(Util::TPageRange::const_iterator it = a_range.begin(); it != a_range.end(); ++it)
    {
        BYTE *pSrc = (BYTE*)it->second;
        BYTE *pDst = pEntry->byData;
        for (int i = 0; i < PAGESIZE; i++)
        {
            *(pDst++) = *(pSrc++) ^ m_header.m_byKey[i % nKeySize];
        }
        pEntry->dwCRC = computeCRC.crc(pEntry->byData, PAGESIZE);
        pEntry++;
    }

    DATAFILEPOS dfpOffset = DATAFILEPOS(m_pageChunk.fpFirst) * PAGESIZE_CRC;

    try
    {
        m_file.Seek(dfpOffset, CFile::begin);
        m_file.Write(m_pageChunk.entries, m_pageChunk.count * PAGESIZE_CRC);
        m_file.SetLength(DATAFILEPOS(m_header.m_fpEnd) * PAGESIZE_CRC);
    }
    catch(CException *e)
    {
        char szMsgBuff[300] = {0};
        e->GetErrorMessage(szMsgBuff, sizeof(szMsgBuff));
        System::setAlarm(
              "DATABASE"
            , Util::message("File=%s,Pages[%d-%d],Message=%s"
            , m_file.GetFileName()
            , m_pageChunk.fpFirst
            , m_pageChunk.fpFirst + m_pageChunk.count - 1
            , szMsgBuff));
        e->Delete();
        return FALSE;
    }
    return TRUE;
}

//*************************************************************************
PAGE_CHUNK_ENTRY* TDataFile::GetEmptyPageBuffer()
{
    if(m_emptyPage.dwCRC == 0)
    {
        BYTE *byBuff = m_emptyPage.byData;

        for(int i = 0; i < PAGESIZE; i++)
        {
            *(byBuff++) ^= m_header.m_byKey[i % m_header.m_byKeySize];
        }
        Util::TComputeCRC computeCRC;
        m_emptyPage.dwCRC = computeCRC.crc(m_emptyPage.byData, PAGESIZE);
    }
    return &m_emptyPage;
}

//*************************************************************************
void TDataFile::LoadPageChunk(FPOINTER a_fpOffset)
{
    if(a_fpOffset >= m_pageChunk.fpFirst && a_fpOffset < (m_pageChunk.fpFirst + m_pageChunk.count))
        return;

    DATAFILEPOS dfpOffset = DATAFILEPOS(a_fpOffset) * PAGESIZE_CRC;
    m_file.Seek(dfpOffset, CFile::begin);
    DWORD dwSize = m_file.Read(m_pageChunk.entries, sizeof(m_pageChunk.entries));

    if(dwSize == 0)
        AfxThrowFileException(CFileException::endOfFile);

    m_pageChunk.fpFirst = a_fpOffset;
    m_pageChunk.count = (FPOINTER)dwSize / PAGESIZE_CRC;

    Util::TComputeCRC computeCRC;

    for(int i = 0; i < m_pageChunk.count; ++i)
    {
        DWORD dwCRC1 = computeCRC.crc(m_pageChunk.entries[i].byData, PAGESIZE);
        if(dwCRC1 != m_pageChunk.entries[i].dwCRC)
            AfxThrowUserException();
    }
}

//*************************************************************************
void TDataFile::ReadPage(FPOINTER a_fpOffset, LPVOID a_pvBuff)
{
    if(a_fpOffset >= m_header.m_fpEnd)
        AfxThrowFileException(CFileException::endOfFile);

    LoadPageChunk(a_fpOffset);

    int index = a_fpOffset - m_pageChunk.fpFirst;
    BYTE *pDst = (BYTE*)a_pvBuff;
    BYTE *pSrc = m_pageChunk.entries[index].byData;
    long nKeySize = long(m_header.m_byKeySize); 

    for (int i = 0; i < PAGESIZE; i++)
    {
        *(pDst++) = *(pSrc++) ^ m_header.m_byKey[i % nKeySize];
    }
}
