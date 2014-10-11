#include "stdafx.h"
#include "TestFramework.h"
#include "TBaseObj.h"

//*********************************************************************************
static void doSectionConcurrencyTest()
{
    int nThread = testing::getThreadIndex();

    CTBSection xCurrentUser;
    xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

    CTBSection xSection;
    xSection.Create(&xCurrentUser, "Test", 0);

    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 0)
    {
        try
        {
            CTBValue val;
            val = "Text";
            xSection.SetValue("Value1", &val);
            xSection.Update();

            testing::setResult(false); // expected exception
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
        CTBValue val;
        val = "Text";
        xSection.SetValue("Value1", &val);
        xSection.Update();

        xSection.DeleteValue("Value1", DELHINT_SIMPLE);
        xSection.Update();
        xSection.Close();

        CTBSection xCurrentUser;
        xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");
        xCurrentUser.DeleteSection("Test");
    }    
}


DEFINE_TEST_CASE(SectionConcurrency, 2, doSectionConcurrencyTest)

//*********************************************************************************
static void doDeleteOpenSectionTest()
{
    CTBSection xSection;

    int nThread = testing::getThreadIndex();

    if(nThread == 0)
    {
        CTBSection xCurrentUser;
        xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

        xSection.Create(&xCurrentUser, "DeleteOpenSection", 0);
    }
    //--------------------------------------------------------------
    testing::threadBarrier();
    //--------------------------------------------------------------
    if(nThread == 1)
    {
        try
        {
            CTBSection xCurrentUser;
            xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");
            xCurrentUser.DeleteSection("DeleteOpenSection");

            testing::setResult(false); // expected exception
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
}

DEFINE_TEST_CASE(DeleteOpenSection, 2, doDeleteOpenSectionTest)
