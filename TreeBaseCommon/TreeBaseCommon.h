#if !defined(TREEBASECOMMON_H)
#define TREEBASECOMMON_H

#ifdef TREEBASECOMMON_EXPORTS
    #define __TBCOMMON_EXPORT__ __declspec(dllexport)
#else
    #define __TBCOMMON_EXPORT__ __declspec(dllexport)
#endif

#define PAGESIZE     2048
#define PAGESIZE_CRC (PAGESIZE+sizeof(DWORD))

#endif TREEBASECOMMON_H