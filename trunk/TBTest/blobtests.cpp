#include "stdafx.h"
#include "TestFramework.h"
#include "TBaseObj.h"
#include "TreeBase.h"

namespace
{
//===================================================================
class CDelayReadMemFile: public CMemFile
{
public:
    virtual UINT Read(void* lpBuf, UINT nCount);
};

//*********************************************************************************
UINT CDelayReadMemFile::Read(void* lpBuf, UINT nCount)
{
    Sleep(1000);
    return CMemFile::Read(lpBuf, nCount);
}

} // namespace

//*********************************************************************************
static void doBlobConcurrencyTest()
{
    int nThread = testing::getThreadIndex();

    CTBSection xCurrentUser;
    xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    CTBSection xSection;
    if(nThread == 0)
    {
        xSection.Create(&xCurrentUser, "BlobTest", 0);
    }
    else
        xSection.Open(&xCurrentUser, "BlobTest");

    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 0)
    {
        BYTE buff[1000] = {0};

        CMemFile data(buff, sizeof(buff));
        
        try
        {
            xSection.SetLongBinary("LB", &data);
            testing::setResult(false);
        }
        catch(CTBException *e)
        {
            if(e->m_errInfo.dwErrorCode != TRERR_SHARING_DENIED)
                testing::setResult(false);
        }
    }
    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 1)
        xSection.Close();

    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 0)
    {
        CDelayReadMemFile data;

        BYTE buff[0x4000] = {0};

        for(int ctr = 0; ctr < 10; ctr++) // 10 sekund
        {
            memset(buff, ctr, sizeof(buff));
            data.Write(buff, sizeof(buff));
        }
        data.SeekToBegin();
        xSection.SetLongBinary("LB1", &data);
    }
    else
    {
        Sleep(2000);
        try
        {
            xSection.Open(&xCurrentUser, "BlobTest");
            testing::setResult(false);
        }
        catch(CTBException *e)
        {
            if(e->m_errInfo.dwErrorCode != TRERR_SHARING_DENIED)
                testing::setResult(false);
        }
    }
    xSection.Close();
    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 0)
    {
        CTBSection xCurrentUser, xSection;
        xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

        xSection.Open(&xCurrentUser, "BlobTest");
        xSection.DeleteValue("LB1", DELHINT_LONGBINARY);
        xSection.Close();

        xCurrentUser.DeleteSection("BlobTest");
    }  
}

DEFINE_TEST_CASE(BlobConcurrency, 2, doBlobConcurrencyTest)


//*********************************************************************************
static void doBlobTempSectionTest()
{
    _asm int 0x3;

/*
    int nThread = testing::getThreadIndex();

    CString strSecName;
    strSecName.Format("BlobTempSectionTest %d", nThread);

    CTBSection xSection;
    {
        CTBSection xCurrentUser;
        xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");
        xSection.Create(&xCurrentUser, strSecName, 0);
    }

    TStream<TBSTREAMOUT> hStreamOut = TBASE_CreateLongBinary(xSection.m_hSection, "LB1");
    ASSERT_NOTNULL(hStreamOut);
    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    BYTE buff[4096] = {0};
    
    for(int i = 0; i < 30; i++)
    {
        ::TBASE_PutData(hStreamOut, buff, sizeof(buff));
    }
    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    hStreamOut.close();

    xSection.DeleteValue("LB1", DELHINT_LONGBINARY);
    xSection.Close();
*/
            
}

DEFINE_TEST_CASE(BlobTempSection, 1/*2*/, doBlobTempSectionTest)
