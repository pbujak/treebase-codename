// blob.h

#if !defined(_BLOB_H_)
#define _BLOB_H_

#include "DataMgr.h"
#include "atomic.h"
#include "stream.h"
#include "section.h"

class TSection;
class TTask;


//==============================================================
class TLongBinary
{
    friend TLongBinary* _GetLongBinary(SEGID_F a_nSegId);

    BOOL            m_bAttached, m_bOpen;
    TAdvancedSegment  m_seg;
    SEGID_F         m_segId;
    TSection       *m_pSection;
    CString         m_strName; 
    int             m_nRefCount, m_nAttachWaiterCount, m_nUnusedWaiterCount;
    THandle         m_hSem, m_hSemUnused;  

    BOOL WaitForAttach();               
public:
	BOOL Rewrite();
	static TLongBinary* OpenLongBinary (TSection *a_pSection, long a_nSegId);
    static void         CloseLongBinary(TLongBinary* a_pLB);
    static BOOL         WaitForSegmentUnused(SEGID_F a_segId);

    TLongBinary(): m_nRefCount(0), m_nAttachWaiterCount(0), m_nUnusedWaiterCount(0), m_bAttached(FALSE), m_bOpen(FALSE)
    {
        m_hSem = CreateSemaphore(NULL, 1, 10, NULL);
    }

    ~TLongBinary();
    BOOL Open(TSection *a_pSection, long a_nSegId);
	BOOL Read (DWORD a_dwPos, DWORD &a_dwSize, LPVOID a_pvBuff);
	BOOL Write(DWORD a_dwPos, DWORD  a_dwSize, LPVOID a_pvBuff);
    BOOL GetStatus(LBSTATUS *a_pStatus);

};

class TLongBinarySectionRef
{
    TSectionData *m_pSectionData;
    bool          m_bAttach, m_bLock;
public:
    TLongBinarySectionRef(TSection *a_pSection);
    ~TLongBinarySectionRef();

    bool attach();
    bool lock();
};

class TLongBinaryInputStream : public Stream::TInputStream
{
    TLongBinary *m_pLB;
    DWORD        m_ofs;
    LBSTATUS     m_status;
public:
    TLongBinaryInputStream(TLongBinary *a_pLB);
    virtual ~TLongBinaryInputStream();
    virtual void* read(DWORD a_cbSize);
    virtual void close();
};

class TLongBinaryOutputStream : public Stream::TOutputStream
{
    TLongBinary *m_pLB;
    DWORD        m_ofs;
public:
    TLongBinaryOutputStream(TLongBinary *a_pLB);
    virtual ~TLongBinaryOutputStream();
    virtual DWORD write(void *a_pvBuff, DWORD a_cbSize);
    virtual bool close();
};


void BLOB_RemoveUncommitted(TSection *a_pTempSection);
TSection* BLOB_CreateBlobTempSection(TSection *a_pSource, LPCSTR a_pszName);
BOOL BLOB_CommitBlob(TSection *a_pSection, LPCSTR a_pszName, LPCSTR a_pszTempSection);

#endif // _BLOB_H_