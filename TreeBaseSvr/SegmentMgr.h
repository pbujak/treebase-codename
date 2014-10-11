#if !defined(SEGMENTMGR_H)
#define SEGMENTMGR_H

#if defined(UNIT_TEST)
#include "UnitTest\TreeBaseSvrTest\include_mock_storage.h"
#include "UnitTest\TreeBaseSvrTest\include_mock_junction.h"
#endif

#include <afxtempl.h>
#include "TreeBase.h"
#include "Storage.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TPageCache.h"
#include <map>

class TBaseFile;

namespace Security
{
    class TSecurityAttributes;
}

namespace Storage
{
    class TAdvPageRef;
}

class TSegmentInitParams
{
public:
    long      m_nId, m_ndbId;
    FPOINTER  m_fpFirst;
};

class TSegment
{
    BOOL M_FixStorage(); 
    RWLOCK* M_GetStorageRWLock();
public:
    BOOL           m_bReadOnly; 
    DWORD          m_dwMask;
    long           m_nId, m_ndbId;
    FPOINTER       m_fpFirst;
    int            m_nPageCount;
    BOOL           m_bAssigned;
    
public:
    FPOINTER GetPageByIndex(int a_nPage);
	BOOL Resize(long a_nPageCount);
    int GetPageCount();
    BOOL Truncate(long a_nPage);
    void* GetPageForRead(long a_nIndex, Storage::TAdvPageRef &a_page);
    void* GetPageForWrite(long a_nIndex, Storage::TAdvPageRef &a_page);
    long AddNewPage(BOOL a_bReserve=FALSE);
    void InitSystemSegment(long a_ndbId, long a_nId);

    inline TSegment()
        : m_bReadOnly(FALSE)
        , m_bAssigned(FALSE)
        , m_ndbId(0)
        , m_nPageCount(-1)
    {
    }

    ~TSegment();
};

template<class _Segment, long _ID>
class TSystemSegment: public _Segment
{
public:
    TSystemSegment(long a_ndbId)
    {
        TSegment::InitSystemSegment(a_ndbId, _ID);
    }
};

long SEGMENT_Create(long a_ndbId, const Security::TSecurityAttributes &a_secAttrs);
BOOL SEGMENT_Assign(long a_ndbId, long a_nSegmentId, TSegment *a_pSegment);
BOOL SEGMENT_AddRef(long a_ndbId, long a_nSegmentId);
BOOL SEGMENT_Delete(long a_ndbId, long a_nSegmentId);
long SEGMENT_GetRefCount(long a_ndbId, long a_nSegmentId);
BOOL SEGMENT_GetSecurity(long a_ndbId, long a_nSegmentId, Security::TSecurityAttributes & a_secAttrs);
BOOL SEGMENT_SetSecurity(long a_ndbId, long a_nSegmentId, const Security::TSecurityAttributes & a_secAttrs);
BOOL SEGMENT_SetSecurityForDomain(long a_ndbIdParent, long a_nSegmentIdParent, 
                                  long a_ndbId, long a_nSegmentId, 
                                  TBSECURITY_OPERATION::Enum operation, 
                                  Security::TSecurityAttributes & a_secAttrs);
BOOL SEGMENT_SetSecurityForSection(long a_ndbIdParent, long a_nSegmentIdParent, 
                                   long a_ndbId, long a_nSegmentId, 
                                   TBSECURITY_OPERATION::Enum operation, 
                                   Security::TSecurityAttributes & a_secAttrs);
long SEGMENT_GetRootSection(long a_ndbId);
long SEGMENT_GetLabelSection(long a_ndbId);
BOOL SEGMENT_AccessCheck(long a_ndbId, long a_nSegmentId, 
                         int a_minimalDesired, int a_maximalDesired, int &a_granted);

BOOL FILE_InitStorage(long a_ndbID, Storage::TStorage *a_pFile);
void FILE_Flush();

class TTrans
{
    Storage::TStorage *m_pStorage;
public:
    TTrans(long a_nFileID);
    TTrans(Storage::TStorage *a_pStorage);
    ~TTrans();
};

#endif //SEGMENTMGR_H
