#ifndef JUNCTION_H
#define JUNCTION_H

#include "Storage.h"

namespace Junction
{
    void addGlobalJunctionPoint(SEGID_F a_src, SEGID_F a_dest);
    void addLocalJunctionPoint(SEGID_F a_src, SEGID_F a_dest);
    bool translateSegmentID(SEGID_F& a_segID);
    bool isJunctionTargetValid(SEGID_F& a_segID);
    void removeJunctionTarget(SEGID_F& a_segID);
};

#endif // JUNCTION_H