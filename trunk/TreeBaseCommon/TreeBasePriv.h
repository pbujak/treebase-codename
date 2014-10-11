// TreeBasePriv.h
#if !defined(TREEBASEPRIV_H)
#define TREEBASEPRIV_H 

class TMemAlloc;

extern TMemAlloc *G_pMalloc;

#define SYS_MemAlloc(size)        (G_pMalloc ? G_pMalloc->Alloc(size)        : NULL)
#define SYS_MemReAlloc(ptr, size) (G_pMalloc ? G_pMalloc->ReAlloc(ptr, size) : NULL)
#define SYS_MemFree(ptr)          (G_pMalloc ? G_pMalloc->Free(ptr)          : NULL)
#define SYS_MemSize(ptr)          (G_pMalloc ? G_pMalloc->MemSize(ptr)       : 0)

void SYS_SetErrorInfo(DWORD a_dwError, LPCSTR a_pszErrorItem=NULL, LPCSTR a_pszSection=NULL);

#endif //TREEBASEPRIV_H