// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <afx.h>

#pragma warning(disable: 4786)
#pragma warning(disable: 4291)

#define BOUNDARY(x, y) ((x + y - 1) / y)

#define IMPLEMENT_INIT(proc) static Util::TInitialize _G_init(&proc);

#define DECLARE_TASK_MEMALLOC() \
static void* operator new(size_t a_cbSize);\
static void operator delete(void *a_pvSegment);

#define IMPLEMENT_TASK_MEMALLOC(class) \
void* class::operator new(size_t a_cbSize)\
{\
    return TASK_MemAlloc(a_cbSize);\
};\
void class::operator delete(void *a_pvSegment)\
{\
    TASK_MemFree(a_pvSegment);\
};

#define _release_ptr(x)\
if(x)\
{\
    DeletePointer(x);\
    x = NULL;\
}

#define stricmp  _stricmp
#define strnicmp _strnicmp
#define fseeko _fseeki64
#define ftello _ftelli64
#define chsize _chsize
#define fileno _fileno
typedef __int64 __off_t;
#define lseeko _lseeki64
#define tello  _telli64

#ifdef UNIT_TEST
    #define ENABLEMOCK virtual
#else
    #define ENABLEMOCK
#endif

typedef DWORD FPOINTER;

template<typename T>
inline void DeletePointer(T *p)
{
    delete p;
};

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
