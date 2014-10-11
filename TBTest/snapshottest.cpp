#include "stdafx.h"
#include "TestFramework.h"
#include "TBaseObj.h"

//*********************************************************************************
static void createSnapshotTestSectrion()
{
    CTBSection xCurrentUser;
    xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

    CTBSection xSnapshotTest;
    xSnapshotTest.Create(&xCurrentUser, "SnapshotTest", 0);
    //--------------------------------------------------------------
    {
        CTBSection xSection1;
        xSection1.Create(&xSnapshotTest, "Section1", 0);
    }
    //--------------------------------------------------------------
    CMemFile memFile;

    for(int i = 0; i < 64; ++i)
    {
        BYTE buff[1024] = {0};
        memset(buff, i, sizeof(buff));
        memFile.Write(buff, sizeof(buff));
    }
    //--------------------------------------------------------------
    xSnapshotTest.SetLongBinary("Blob1", &memFile);

    for(int i = 0; i < 200; ++i)
    {
        CTBValue vName("Name");
        CTBValue vAge(i);
        
        CString name, age;
        name.Format("Name %d", i);
        age.Format("Age %d", i);

        xSnapshotTest.SetValue(name, &vName);
        xSnapshotTest.SetValue(age, &vAge);
    }
    //--------------------------------------------------------------
    xSnapshotTest.Update();
}

//*********************************************************************************
static void openSnapshotTestSectrion()
{
    CTBSection xCurrentUser;
    xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

    CTBSection xSnapshotTest;
    xSnapshotTest.Open(&xCurrentUser, "SnapshotTest", TBOPEN_MODE_SNAPSHOT);
    
    CString strName;
    DWORD dwType = 0;
    int nTop = 0;

    BOOL more = xSnapshotTest.GetFirstItem(strName, dwType);
    while(more)
    {
        TEST_VERIFY((dwType == TBVTYPE_LONGBINARY) == (strName == "Blob1"));
        TEST_VERIFY((dwType == TBVTYPE_SECTION) == (strName == "Section1"));

        if(dwType != TBVTYPE_LONGBINARY && dwType != TBVTYPE_SECTION)
        {
            CTBValue value;
            CString prefix;
            long index = 0;
            sscanf(strName, "%s %d", prefix.GetBuffer(10), &index);

            if(prefix == "Name")
            {
                if(nTop < index) nTop = index;
                TEST_VERIFY(dwType == TBVTYPE_TEXT);
                xSnapshotTest.GetValue(strName, &value);
                TEST_VERIFY(strcmp(value.m_pValue->data.text, "Name") == 0);
            }
            else if(prefix == "Age")
            {
                if(nTop < index) nTop = index;
                TEST_VERIFY(dwType == TBVTYPE_INTEGER);
                xSnapshotTest.GetValue(strName, &value);
                TEST_VERIFY(value.m_pValue->data.int32 == index);
            }
        }
        more = xSnapshotTest.GetNextItem(strName, dwType);
    }

    TEST_VERIFY(nTop == 199);

    CTBSection xSection1;
    xSection1.Open(&xSnapshotTest, "Section1");
    xSection1.Close();

    //--------------------------------------------------------------
    CMemFile memFile;
    xSnapshotTest.GetLongBinary("Blob1", &memFile);
    memFile.SeekToBegin();

    for(int i = 0; i < 64; ++i)
    {
        BYTE buff[1024] = {0};
        BYTE buff2[1024] = {0};

        memset(buff2, i, sizeof(buff2));
        memFile.Read(buff, sizeof(buff));

        TEST_VERIFY(memcmp(buff, buff2, 1024) == 0);
    }
    //--------------------------------------------------------------
    TEST_VERIFY(memFile.GetPosition() == memFile.GetLength());

    xSnapshotTest.Close();
}

//*********************************************************************************
static void deleteSnapshotTestSectrion()
{
    CTBSection xCurrentUser;
    xCurrentUser.Open(&CTBSection::tbRootSection, "CurrentUser");

    CTBSection xSnapshotTest;
    xSnapshotTest.Open(&xCurrentUser, "SnapshotTest", TBOPEN_MODE_DYNAMIC);

    CString strName;
    DWORD dwType = 0;
    BOOL more = xSnapshotTest.GetFirstItem(strName, dwType);
    while(more)

    {
        switch(dwType)
        {
            case TBVTYPE_SECTION:
                xSnapshotTest.DeleteSection(strName);
                break;
            default:
                xSnapshotTest.DeleteValue(
                    strName, 
                    dwType == TBVTYPE_LONGBINARY ? DELHINT_LONGBINARY : DELHINT_SIMPLE
                );
                break;
        }

        more = xSnapshotTest.GetNextItem(strName, dwType);
    }

    xSnapshotTest.Update();
    xSnapshotTest.Close();

    xCurrentUser.DeleteSection("SnapshotTest");
}

//*********************************************************************************
static void doSnapshotTest()
{
    createSnapshotTestSectrion();
    openSnapshotTestSectrion();
    deleteSnapshotTestSectrion();
}

DEFINE_TEST_CASE(Snapshot, 1, doSnapshotTest)
