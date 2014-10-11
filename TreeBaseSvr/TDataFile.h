#pragma once

#include <set>
#include "TreeBaseCommon.h"

#define CHUNK_SIZE      16
#define CHUNK_SIZE_MASK (~(FPOINTER)15)

#define PAGES_OF_HEADER BOUNDARY(sizeof(FILEHEADER), PAGESIZE)

typedef unsigned __int64 QWORD;
typedef long             BOOL32;

typedef LONGLONG DATAFILEPOS;

typedef struct _tagFILEHEADER
{
    BYTE     m_byKey[128];
    long     m_byKeySize;
    FPOINTER m_fpSegmentDirPage;
    FPOINTER m_fpSecDataPage;
    long     m_RootSectionSegId;
    long     m_LabelSectionSegId;
    long     m_TempLBSectionSegId;
    BOOL32   m_bDirty;
    BOOL32   m_bTrans;
    BOOL32   m_bNeedCompact;
    FPOINTER m_fpEnd;
    BOOL32   m_nPageTabDirCount;
}FILEHEADER;

typedef struct _PAGE_CHUNK_ENTRY
{
    BYTE        byData[PAGESIZE];  
    DWORD       dwCRC;
}PAGE_CHUNK_ENTRY;

typedef struct _PAGE_CHUNK
{
    FPOINTER         fpFirst;
    int              count;
    PAGE_CHUNK_ENTRY entries[CHUNK_SIZE];
}PAGE_CHUNK;

namespace Util
{
    class TPageRangeIterator;
    class TPageRange;
}

namespace Storage
{
    //========================================
    class TDataFile
    {
        CFile             m_file, m_fileControl;
        FILEHEADER        m_header;
        BOOL              m_bReadOnly;
        PAGE_CHUNK        m_pageChunk;
        PAGE_CHUNK_ENTRY  m_emptyPage;

    private:
        void              LoadPageChunk(FPOINTER a_fpOffset);
        PAGE_CHUNK_ENTRY* GetEmptyPageBuffer();

    public:
        TDataFile(void);
    public:
        ~TDataFile(void);

        BOOL Open(LPCSTR a_pszFileName);
        void InitKey();
        void Close();

        BOOL BeginWrite(Util::TPageRangeIterator *a_pSource);
        void EndWrite();
        BOOL GrowFile(DWORD a_dwNewSize);
        BOOL WriteHeader();

        FILEHEADER* GetHeader()
        {
            return &m_header;
        }
        BOOL IsReadOnly()
        {
            return m_bReadOnly;
        }
        void Recover();
        void ReadPage(FPOINTER a_fpOffset, LPVOID a_pvBuff);
        BOOL WritePages(Util::TPageRange &a_range);
    };

}