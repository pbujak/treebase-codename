#include "stdafx.h"
#include "TBaseFile.h"
#include "section.h"
#include "LockPath.h"
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

struct TMountPoint
{
    CString m_strName;
    CString m_strMntSection;
    CString m_strDirectory;
    CString m_strFile;
    BOOL    m_bAuto;
    BOOL    m_bCheckMedia;
};

//************************************************************************
static SEGID_F _ResolvePath(LPCSTR a_pszPath, TIDArray &a_IDsPath)
{
    TSection xRoot;
    CString  strFullPath;
    CString  strPath(a_pszPath);

    xRoot.m_ndbId  = 0;
    xRoot.m_nSegId = SEGMENT_GetRootSection(0);
    xRoot.m_arrPathIDs.M_Add(0, xRoot.m_nSegId);

    if(strPath.IsEmpty() || strPath[0]!='\\')
        return SEGID_NULL;

    strPath.Delete(0);

    return TSection::ResolvePath(&xRoot, strPath, strFullPath, &a_IDsPath);
};

//************************************************************************
static BOOL _AddMountPoint(TMountPoint &a_mntPoint)
{
    SEGID_F  segID;
    TIDArray arrIDsPath;
    long     ndbID = -1;
    char     szPath[MAX_PATH] = {0};
    TDBFileEntry *pEntry = NULL;

    // filesys path
    PathCombine(szPath, a_mntPoint.m_strDirectory, a_mntPoint.m_strFile);

    // TBASE path
    segID = _ResolvePath(a_mntPoint.m_strMntSection, arrIDsPath);
    if(!SEGID_IS_NULL(segID))
        ndbID = G_dbManager.AddEntry(segID, a_mntPoint.m_strName, szPath);

    if(ndbID!=-1)
    {
        a_mntPoint.m_strMntSection += "\\";
        LOCKPATH_AddPath(a_mntPoint.m_strMntSection, arrIDsPath);
        pEntry = G_dbManager[ndbID];
        if(pEntry)
        {
            pEntry->m_bCheckMedia = a_mntPoint.m_bCheckMedia;
        }
        return TRUE;
    }

    return FALSE;
};

//************************************************************************
BOOL CONFIG_Init()
{
//    TMountPoint mntPoint;
//    mntPoint.m_strName       = "FLOPPY";
//    mntPoint.m_strMntSection = "\\External\\FLOPPY";
//    mntPoint.m_strDirectory  = "A:\\Data\\TreeBase";
//    mntPoint.m_strFile       = "treebase.dat";
//    mntPoint.m_bCheckMedia   = TRUE;
//    _AddMountPoint(mntPoint);
//    FILE_Mount("FLOPPY", TRUE);
    return TRUE;
}