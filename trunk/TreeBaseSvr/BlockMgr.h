#if !defined(BLOCKMGR_H)
#define BLOCKMGR_H

#if defined(UNIT_TEST)
#include "UnitTest\TreeBaseSvrTest\include_mock_storage.h"
#endif

#include <afxtempl.h>
#include "TreeBase.h"
#include "Storage.h"
#include "TAbstractThreadLocal.h"
#include "TPageCache.h"
#include <map>

typedef struct
{
    DWORD dwBlockID;
    DWORD dwFirstPage;
    DWORD dwBlockMask;
    WORD  wRefCount;
    ACCESS_TOKEN token;
}BLOCK_ENTRY;

#define BLOCKS_PER_PAGE (PAGESIZE / sizeof(BLOCK_ENTRY))

class TBaseFile;

namespace Storage
{
class TMultiPageRef;
}

class TBlockInitParams
{
public:
    long      m_nId, m_ndbId;
    FPOINTER  m_fpFirst;
};

class TBlock
{
    BOOL M_FixStorage(); 
    RWLOCK* M_GetStorageRWLock();
public:
    BOOL           m_bReadOnly; 
    DWORD          m_dwMask;
    long           m_nId, m_ndbId;
    FPOINTER       m_fpFirst;
    int            m_nPageCount;
    ACCESS_TOKEN   m_token;
    BOOL           m_bAssigned;
    
public:
    FPOINTER GetPageByIndex(int A_nPage);
	BOOL Resize(long A_nPageCount);
    int GetPageCount();
    BOOL Truncate(long A_nPage);
    void* GetPageForRead(long A_nIndex, Storage::TMultiPageRef &A_page);
    void* GetPageForWrite(long A_nIndex, Storage::TMultiPageRef &A_page);
    long AddNewPage(BOOL A_bReserve=FALSE);
    void InitSystemBlock(long A_ndbId, long A_nId);

    inline TBlock()
        : m_bReadOnly(FALSE)
        , m_bAssigned(FALSE)
        , m_ndbId(0)
        , m_nPageCount(-1)
    {
        memset(&m_token, 0, sizeof(ACCESS_TOKEN));
    }
};

template<class _Block, long _ID>
class TSystemBlock: public _Block
{
public:
    TSystemBlock(long a_ndbId)
    {
        TBlock::InitSystemBlock(a_ndbId, _ID);
    }
};

long BLOCK_Create(long A_ndbId, DWORD A_dwMask);
BOOL BLOCK_Assign(long A_ndbId, long A_nBlockId, TBlock *A_pBlock, BOOL &A_bReadOnly);
BOOL BLOCK_AddRef(long A_ndbId, long A_nBlockId);
BOOL BLOCK_Delete(long A_ndbId, long A_nBlockId);
long BLOCK_GetRefCount(long A_ndbId, long A_nBlockId);
long BLOCK_GetRootSection(long A_ndbId);
long BLOCK_GetLabelSection(long A_ndbId);

BOOL FILE_InitStorage(long A_ndbID, Storage::TStorage *A_pFile);
void FILE_Flush();

class TTrans
{
    Storage::TStorage *m_pStorage;
public:
    TTrans(long A_nFileID);
    TTrans(Storage::TStorage *a_pStorage);
    ~TTrans();
};

#endif //BLOCKMGR_H