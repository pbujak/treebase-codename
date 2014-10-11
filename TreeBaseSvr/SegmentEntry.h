#ifndef _SEGMENT_ENTRY_H
#define _SEGMENT_ENTRY_H

#include "SecurityManager.h"

typedef struct
{
    DWORD dwSegmentID;
    DWORD dwFirstPage;
    WORD  wRefCount;
    Security::REF securityRef;
}SEGMENT_ENTRY;

#define SEGMENTS_PER_PAGE (PAGESIZE / sizeof(SEGMENT_ENTRY))


#endif // _SEGMENT_ENTRY_H