#if !defined(__INCLUDE_MOCK_SEGMENT__)
#define __INCLUDE_MOCK_SEGMENT__

#define SEGMENTMGR_H
#define TBASEFILE_H
#define TASK_H
#define TPAGECACHE_H

#include "MockStorage.h"
#include "MockDBManager.h"
#include "MockTask.h"
#include "TMockSegment.h"
#include "TreeBaseInt.h"
#include "TPageCache.h"

#define TSegment TMockSegment
#define IMPLEMENT_INIT(proc) static TInitialize _G_init(&proc);

#define TPageRef TMockPageRef
#define TPagePool TMockPagePool

#endif // __INCLUDE_MOCK_SEGMENT__
