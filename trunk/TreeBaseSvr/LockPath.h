// LockPath.h

#if !defined(_LOCKPATH_)
#define _LOCKPATH_

#include "Section.h"

void LOCKPATH_RemovePath(LPCSTR a_pszPath, TIDArray &a_Array);
BOOL LOCKPATH_WaitForEntity(SEGID_F a_segId, LPCSTR a_pszSubName);
void LOCKPATH_AddPath(LPCSTR a_pszPath, TIDArray &a_Array);

#endif // _LOCKPATH_