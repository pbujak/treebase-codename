#include "stdafx.h"
#include "TSectionSnapshot.h"
#include "DataMgr.h"
#include "SecurityManager.h"
#include "TBaseFile.h"

//********************************************************************
TSectionSnapshot* TSectionSnapshot::create(TSectionSegment *a_pSegment)
{
    TSectionSnapshot *pSnapshot = new TSectionSnapshot();

    if(!pSnapshot->initialize(a_pSegment))
    {
        delete pSnapshot;
        return NULL;
    }
    return pSnapshot;
}

//********************************************************************
int TSectionSnapshot::createTempSegment()
{
    int segId = SEGMENT_Create(FILE_dbIdTemp, &Security::Manager::TempSecurity);
    m_tempSegments.push_back(segId);
    return segId;
}

//********************************************************************
bool TSectionSnapshot::initialize(TSectionSegment *a_pSegment)
{
    snapshotSegId.ndbId = FILE_dbIdTemp;
    snapshotSegId.nSegId = createTempSegment();

    sourceSegId.ndbId = a_pSegment->m_ndbId;
    sourceSegId.nSegId = a_pSegment->m_nId;

    int effPageSize = COMMON_PageSizeAdjustedToFibonacci();

    BlobMap blobMap;
    //----------------------------------------------------------
    {
        TSegment target;

        if(!SEGMENT_Assign(FILE_dbIdTemp, snapshotSegId.nSegId, &target))
            return false;

        BYTE buff[PAGESIZE] = {0};

        int nPage = 0;
        while(Util::Segment::readPage(a_pSegment, nPage, buff))
        {
            int nOffset = 0;
            while(nOffset < effPageSize)
            {
                TBITEM_ENTRY *pEntry = MAKE_PTR(TBITEM_ENTRY*, buff, nOffset);

                if(TBITEM_TYPE(pEntry) == TBVTYPE_LONGBINARY)
                {
                    TBVALUE_ENTRY *pValue = TBITEM_VALUE(pEntry);
                    int tempBlob = createTempSegment();
                    blobMap[pValue->int32] = tempBlob;
                    pValue->int32 = tempBlob;
                }

                nOffset += pEntry->wSize;
            }

            Util::Segment::writePage(&target, nPage, buff);
            nPage++;
        }
    }
    //----------------------------------------------------------
    // copy BLOBs
    for(BlobMapIter it = blobMap.begin(); it != blobMap.end(); ++it)
    {
        TSegment source, target; 

        SEGMENT_Assign(sourceSegId.ndbId, it->first,  &source);
        SEGMENT_Assign(FILE_dbIdTemp,     it->second, &target);

        BYTE buff[PAGESIZE] = {0};

        int nPage = 0;
        while(Util::Segment::readPage(&source, nPage, buff))
        {
            Util::Segment::writePage(&target, nPage, buff);
            nPage++;
        }
    }
    return true;
}

//********************************************************************
void TSectionSnapshot::translateSegmentID(SEGID_F &a_segID)
{
    if(a_segID.ndbId == snapshotSegId.ndbId)
        a_segID.ndbId = sourceSegId.ndbId;
}

//********************************************************************
TSectionSnapshot::~TSectionSnapshot(void)
{
    for(std::vector<int>::const_iterator it = m_tempSegments.begin(); it != m_tempSegments.end(); ++it)
    {
        SEGMENT_Delete(FILE_dbIdTemp, *it);
    }
}
