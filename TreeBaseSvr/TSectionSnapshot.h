#ifndef _TSECTIONSNAPSHOT_H_
#define _TSECTIONSNAPSHOT_H_

#include <vector>
#include <map>
#include "Storage.h"

class TSectionSegment;

class TSectionSnapshot
{
    typedef std::map<int, int> BlobMap;
    typedef BlobMap::const_iterator BlobMapIter;

private:
    SEGID_F snapshotSegId, sourceSegId;

    std::vector<int> m_tempSegments;

private:
    TSectionSnapshot(void) {};

    int createTempSegment();
    bool initialize(TSectionSegment *a_pSegment);
public:
    static TSectionSnapshot* create(TSectionSegment *a_pSegment);
    ~TSectionSnapshot(void);

    void translateSegmentID(SEGID_F &a_segID);
    inline int dbId()  { return snapshotSegId.ndbId; }
    inline int segId() { return snapshotSegId.nSegId; }
};

#endif _TSECTIONSNAPSHOT_H_