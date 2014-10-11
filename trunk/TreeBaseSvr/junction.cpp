#include "stdafx.h"
#include "junction.h"
#include "Shared\TAbstractThreadLocal.h"
#include "syncobj.h"
#include <afxtempl.h>

typedef CMap<SEGID_F, SEGID_F&, SEGID_F, SEGID_F&> TJunctionMap;

inline BOOL operator==(const SEGID_F& a, const SEGID_F& b)
{
    return a.ndbId == b.ndbId && a.nSegId == b.nSegId;
};

static TJunctionMap                s_global, s_targets;
static TThreadLocal<TJunctionMap>  s_local;
static TCriticalSection            s_cs; 

template<> UINT AFXAPI HashKey<SEGID_F&>(SEGID_F& a_rSegIdf);

//************************************************************************
bool Junction::isJunctionTargetValid(SEGID_F& a_segID)
{
    if(a_segID.ndbId == -1)
        return false;

    TCSLock _lock(&s_cs);

    return s_targets.Lookup(a_segID, a_segID) == TRUE;
}

//************************************************************************
void Junction::removeJunctionTarget(SEGID_F& a_segID)
{
    TCSLock _lock(&s_cs);
    s_targets.RemoveKey(a_segID);
}

//************************************************************************
void Junction::addGlobalJunctionPoint(SEGID_F a_src, SEGID_F a_dest)
{
    TCSLock _lock(&s_cs);
    s_global[a_src] = a_dest;

    if(a_dest.ndbId != -1)
        s_targets[a_dest] = a_dest;
}

//************************************************************************
void Junction::addLocalJunctionPoint(SEGID_F a_src, SEGID_F a_dest)
{
    TJunctionMap &refLocal = s_local.Value();
    refLocal[a_src] = a_dest;

    TCSLock _lock(&s_cs);
    if(a_dest.ndbId != -1)
        s_targets[a_dest] = a_dest;
}

//************************************************************************
bool Junction::translateSegmentID(SEGID_F& a_segID)
{
    TJunctionMap &refLocal = s_local.Value();

    SEGID_F segID = a_segID;

    while(true)
    {
        SEGID_F segID2;
        BOOL    bFound = TRUE;

        if(!refLocal.Lookup(segID, segID2))
        {
            TCSLock _lock(&s_cs);
            bFound = s_global.Lookup(segID, segID2);
        }

        if(!bFound)
        {
            bool wasChanged = !(a_segID == segID);
            a_segID = segID;
            return !wasChanged || isJunctionTargetValid(segID);
        }
        segID = segID2;
    }
}
