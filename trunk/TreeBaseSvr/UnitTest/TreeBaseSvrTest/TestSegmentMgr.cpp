#include "stdafx.h"
#include "defs.h"
#include "..\..\TStoragePageAlloc.h"
#include "..\..\TSegmentPageIndexMap.h"
#include "..\..\TSecurityAttributes.h"
#include "..\..\SegmentMgr.h"
#include "..\..\compact.h"
#include "MockDBManager.h"
#include "MockStorage.h"
#include <vector>
#include <set>

typedef struct{
    int nBase;
    int nAmpl;
    int nCount;
    long segId;
}TestSegmentInfo;

//=======================================================================
class TTestCompactBase: public TCompactBase
{
    int    m_nCounter;
protected:
    virtual BOOL IsOverTime();
    virtual void YieldControl();
public:
    inline TTestCompactBase(): m_nCounter(0) {};
};

//***********************************************************************
BOOL TTestCompactBase::IsOverTime()
{
    return ++m_nCounter > 5;
}

//***********************************************************************
void TTestCompactBase::YieldControl()
{
    m_nCounter = 0;
}


//***********************************************************************
static BOOL InitStorage(Storage::TStorage *a_pStorage)
{
    FILEHEADER hdr = {0};
    hdr.m_nPageTabDirCount = 16;
    a_pStorage->setHeader(&hdr);

    TASK_FixStorage(a_pStorage);
    if(!FILE_InitStorage(0, a_pStorage))
        return FALSE;

    return TRUE;
}

//***********************************************************************
static void pushSegmentPages(TSegment *a_pSegment, std::set<FPOINTER> &a_setPages)
{
    int nCount = a_pSegment->GetPageCount();

    for(int i = 0; i < nCount; i++)
    {
        FPOINTER fpPage = a_pSegment->GetPageByIndex(i);
        a_setPages.insert(fpPage);
    }
}

//***********************************************************************
static void markSegment(TSegment *a_pSegment, int a_nIndex)
{
    Storage::TAdvPageRef xPageRef;

    int nCount = a_pSegment->GetPageCount();

    for(int page = 0; page < nCount; page++)
    {
        BYTE *buff = (BYTE*)a_pSegment->GetPageForWrite(page, xPageRef);

        int i = 0;
        do
        {
            buff[i++] = a_nIndex;
            buff[i++] = page;
        }
        while(i < PAGESIZE);
    }
}

//***********************************************************************
static void checkSegment(TSegment *a_pSegment, int a_nIndex)
{
    Storage::TAdvPageRef xPageRef;

    int nCount = a_pSegment->GetPageCount();

    for(int page = 0; page < nCount; page++)
    {
        BYTE *buff = (BYTE*)a_pSegment->GetPageForRead(page, xPageRef);

        int i = 0;
        do
        {
            TEST_VERIFY(buff[i++] == a_nIndex);
            TEST_VERIFY(buff[i++] == page);
        }
        while(i < PAGESIZE);
    }
}

//***********************************************************************
TEST_ENTRY(SegmentMgr_SegmentResize, "Segment Resize")
{
    BOOL bReadOnly = FALSE;
    Storage::TStorage storage(FALSE);
    if(!InitStorage(&storage))
        return false;

    //------------------------------------------------
    Security::TSecurityAttributes secAttrs(TBACCESS_WRITE, TBACCESS_WRITE, TBACCESS_WRITE);
    //------------------------------------------------
    long segId = SEGMENT_Create(0, secAttrs);
    {
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));

        objSeg.Resize(6);
    }
    //----------------------------------------------
    {
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));

        int nSize = objSeg.GetPageCount();
        TEST_VERIFY(nSize == 6)

        objSeg.Truncate(3);
    }
    //------------------------------------------------
    {
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));
        TEST_VERIFY(objSeg.GetPageCount() == 3);
    }
    return true;
}

//***********************************************************************
TEST_ENTRY(SegmentMgr_SegmentData, "Multiple Segment Data Write")
{
    Security::TSecurityAttributes secAttrs(TBACCESS_WRITE, TBACCESS_WRITE, TBACCESS_WRITE);

    Storage::TStorage storage(FALSE);
    TEST_VERIFY(InitStorage(&storage));

    std::vector<TestSegmentInfo> vecSegmentInfo;
    std::vector<TestSegmentInfo>::iterator iter;

    for(int i = 0; i < 100; i++)
    {
        TestSegmentInfo bi;
        bi.nBase = rand() % 71;
        bi.nAmpl = rand() % 219 + 3;
        bi.nCount = rand() % 17 + 50;
        vecSegmentInfo.push_back(bi);
    }

    for(iter = vecSegmentInfo.begin(); iter != vecSegmentInfo.end(); iter++)
    {
        Storage::TAdvPageRef xPageRef;
        TestSegmentInfo &bi = *iter;
        bi.segId = SEGMENT_Create(0, secAttrs);

        BOOL bReadOnly = FALSE;
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, bi.segId, &objSeg));

        int val = bi.nBase;
        objSeg.Resize(bi.nCount);
        for(int nPage = 0; nPage < bi.nCount; nPage++)
        {
            BYTE *byData = (BYTE *)objSeg.GetPageForWrite(nPage, xPageRef);
            for(int ctr = 0; ctr < PAGESIZE; ctr++)
            {
                byData[ctr] = val;
                val = (val + 1) % bi.nAmpl;
            }
        }
    }

    for(iter = vecSegmentInfo.begin(); iter != vecSegmentInfo.end(); iter++)
    {
        Storage::TAdvPageRef xPageRef;
        TestSegmentInfo &bi = *iter;
        BOOL bReadOnly = FALSE;
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, bi.segId, &objSeg));

        TEST_VERIFY(bi.nCount == objSeg.GetPageCount());

        int val = bi.nBase;
        for(int nPage = 0; nPage < objSeg.GetPageCount(); nPage++)
        {
            BYTE *byData = (BYTE *)objSeg.GetPageForRead(nPage, xPageRef);
            for(int ctr = 0; ctr < PAGESIZE; ctr++)
            {
                TEST_VERIFY(byData[ctr] == val);
                val = (val + 1) % bi.nAmpl;
            }
        }
    }

    return true;
}

//***********************************************************************
TEST_ENTRY(SegmentMgr_MultiDelete, "Multiple Segment Delete")
{
    Storage::TStorage storage(FALSE);
    TEST_VERIFY(InitStorage(&storage));

    std::vector<int> vecSegId;
    std::set<FPOINTER> sysSegmentPages;

    for(int i = 0; i < 200; i++)
    {
        Storage::TAdvPageRef xPageRef;
        int segId = SEGMENT_Create(0, Security::TSecurityAttributes(TBACCESS_WRITE, TBACCESS_WRITE, TBACCESS_WRITE));
        vecSegId.push_back(segId);

        BOOL bReadOnly = FALSE;
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));

        objSeg.Resize(i < 60 ? i + 4 : i - 50);
    }

    std::vector<int>::iterator it;
    for(it = vecSegId.begin(); it != vecSegId.end(); ++it)
    {
        int nSegId = *it;
        SEGMENT_Delete(0, nSegId);
    }

    FILEHEADER hdr = storage.getHeader();

    TSystemSegment<TSegment, SEGID_SEGMENTDIR> segmentDir(0);
    pushSegmentPages(&segmentDir, sysSegmentPages);

    TSystemSegment<TSegment, SEGID_SECDATA> secData(0);
    pushSegmentPages(&secData, sysSegmentPages);
    //-------------------------------------------------------------------------
    {
        TSegment segRoot;
        BOOL bReadOnly = FALSE;
        TEST_VERIFY(SEGMENT_Assign(0, hdr.m_RootSectionSegId, &segRoot));
        pushSegmentPages(&segRoot, sysSegmentPages);
    }
    //-------------------------------------------------------------------------
    {
        TSegment segLabel;
        BOOL bReadOnly = FALSE;
        TEST_VERIFY(SEGMENT_Assign(0, hdr.m_LabelSectionSegId, &segLabel));
        pushSegmentPages(&segLabel, sysSegmentPages);
    }
    //-------------------------------------------------------------------------
    {
        TSegment segTemp;
        BOOL bReadOnly = FALSE;
        TEST_VERIFY(SEGMENT_Assign(0, hdr.m_TempLBSectionSegId, &segTemp));
        pushSegmentPages(&segTemp, sysSegmentPages);
    }
    //-------------------------------------------------------------------------
    // Scan page table
    for(FPOINTER fpPage = 0; fpPage < hdr.m_fpEnd; fpPage++)
    {
        Storage::TAdvPageRef xPageRef;

        FPOINTER fpTable = 0;
        int      nPageIndex = 0;

        Storage::TStoragePageAlloc::GetTableAndPageIndex(fpPage, fpTable, nPageIndex, xPageRef);

        PAGE_ENTRY *pEntry  = (PAGE_ENTRY *)xPageRef.getPageForRead(fpTable);

        if(fpPage < hdr.m_nPageTabDirCount + 1)
        {
            TEST_VERIFY(pEntry[nPageIndex].dwSegmentId == SEGID_RESERVED);
        }
        else if(fpPage >= fpTable && fpPage < fpTable + SUBTABLE_COUNT)
        {
            TEST_VERIFY(pEntry[nPageIndex].dwSegmentId == SEGID_TABLE);
        }
        else if(sysSegmentPages.find(fpPage) == sysSegmentPages.end())
        {
            TEST_VERIFY(pEntry[nPageIndex].dwSegmentId == 0);
        }
    }

    return true;
}

//***********************************************************************
TEST_ENTRY(SegmentMgr_Compact, "Compact")
{
    Storage::TStorage storage(FALSE);
    TEST_VERIFY(InitStorage(&storage));

    std::vector<int>   vecSegId;
    std::map<int, int> mapSegId;

    //----------------------------------------------------------
    for(int i = 0; i < 70; i++)
    {
        Storage::TAdvPageRef xPageRef;
        int segId = SEGMENT_Create(0, Security::TSecurityAttributes(TBACCESS_WRITE, TBACCESS_WRITE, TBACCESS_WRITE));
        vecSegId.push_back(segId);

        BOOL bReadOnly = FALSE;
        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));

        objSeg.Resize(90);
        mapSegId[segId] = 90;
        markSegment(&objSeg, segId);
    }
    //----------------------------------------------------------
    typedef std::map<int, int>::iterator Iter;

    for(Iter it = mapSegId.begin(); it != mapSegId.end(); ++it)
    {
        int segId = it->first;
        if((segId % 8) == 0)
        {
            BOOL bReadOnly = FALSE;
            TSegment objSeg;
            TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));
            objSeg.Resize(3);
            it->second = 3;
        }
        if((segId % 13) == 0)
        {
            TEST_VERIFY(SEGMENT_Delete(0, segId));
            it = mapSegId.erase(it);
        }
    }
    //----------------------------------------------------------
    TTestCompactBase compact;
    TEST_VERIFY(compact.Compact(0));
    //----------------------------------------------------------
    for(Iter it = mapSegId.begin(); it != mapSegId.end(); ++it)
    {
        int segId = it->first;
        int size  = it->second;
        BOOL bReadOnly = FALSE;

        TSegment objSeg;
        TEST_VERIFY(SEGMENT_Assign(0, segId, &objSeg));

        TEST_VERIFY(objSeg.GetPageCount() == size);
        checkSegment(&objSeg, segId);
    }
    //----------------------------------------------------------
    return true;
}
