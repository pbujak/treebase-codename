/* 
 * File:   TTempFile.cpp
 * Author: pabu
 * 
 * Created on 28 January 2013, 11:24
 */

#include "stdafx.h"
#include "TTempFile.h"
#include "Util.h"
#include <assert.h>
#include <cstring>
#include <io.h>
#include <stdio.h>
#include <sys\stat.h>
#include <fcntl.h>
#include <exception>

#define ID_MARKER 0x908F

#define FILE_REGION_SIZE s_nRegionSize
#define FILE_REGION_MASK s_nRegionMask
#define FILE_GROW_UNIT   s_nGrowUnit

using namespace Storage;

static int       s_nRegionSize;
static OFFSET_FT s_nRegionMask;
static int       s_nGrowUnit;

//******************************************************************************
static BOOL InitConstants()
{
    SYSTEM_INFO si = {0};
    GetSystemInfo(&si);

    s_nRegionSize = si.dwAllocationGranularity;
    s_nRegionMask = s_nRegionSize - 1;
    s_nGrowUnit   = s_nRegionSize << 2;

    return TRUE;
};

IMPLEMENT_INIT(InitConstants)

//******************************************************************************
bool TGenericTempFile::MMFile::open(const char *a_pszPath)
{
    m_hFile = CreateFile(
       a_pszPath, 
       GENERIC_READ | GENERIC_WRITE, 
       0, NULL, CREATE_ALWAYS,
       FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY, 
       NULL);

    if(!m_hFile)
        return false;
    m_size = 0;
    SetFilePointer(m_hFile, 0, 0, FILE_BEGIN);
    SetEndOfFile(m_hFile);

    return true;
}

//******************************************************************************
void TGenericTempFile::MMFile::grow()
{
    m_hFileMapping = NULL;

    m_size += FILE_GROW_UNIT;
    m_hFileMapping = CreateFileMapping(m_hFile, NULL, PAGE_READWRITE, 0, (DWORD)m_size, NULL);

    if(m_hFileMapping == NULL)
        throw std::exception();
}

//******************************************************************************
void* TGenericTempFile::MMFile::mmap(OFFSET_FT a_ofsBase)
{
    assert(m_hFileMapping != NULL);
    void* pvAddr = MapViewOfFile(m_hFileMapping, FILE_MAP_WRITE, 0, (DWORD)a_ofsBase, FILE_REGION_SIZE);
    if(pvAddr == NULL)
        throw std::exception();

    return pvAddr;
}

//******************************************************************************
void TGenericTempFile::MMFile::munmap(void *a_pvAddr)
{
    VERIFY(UnmapViewOfFile(a_pvAddr));
}

//******************************************************************************
void TGenericTempFile::MMFile::reset()
{
    m_hFileMapping = NULL;
    m_size = 0;

    SetFilePointer(m_hFile, 0, 0, FILE_BEGIN);
    SetEndOfFile(m_hFile);
}

//******************************************************************************
TGenericTempFile::MMFile::~MMFile()
{
    m_hFileMapping = NULL;
    m_hFile = NULL;
}

//******************************************************************************
void* TGenericTempFile::Cache::findRegion(OFFSET_FT a_ofsBase)
{
    FILE_REGION_DESCRIPTOR *pRegion = findEntry(a_ofsBase);
    
    return pRegion ? pRegion->pvMem : NULL;
}

//******************************************************************************
void* TGenericTempFile::Cache::addRegion(OFFSET_FT a_ofsBase, TTempFile::MMFile &a_file)
{
    void *pvAddr = a_file.mmap(a_ofsBase);
    if(pvAddr)
    {
        FILE_REGION_DESCRIPTOR *pRegion = addNewEntry(a_ofsBase);
        pRegion->ofsBase = a_ofsBase;
        pRegion->pvMem = pvAddr;
        return pvAddr;
    }
    return NULL;
}

//******************************************************************************
TTempFile::TTempFile(): m_pageInfoPool(sizeof(PAGE_INFO_TMP))
{
    for(int ctr = 0; ctr < PAGE_INFO_HASH_COUNT; ctr++)
    {
        m_hash[ctr] = -1;
    }
}

//******************************************************************************
TTempFile::~TTempFile() 
{
}

//******************************************************************************
OFFSET_FT TTempFile::allocOffset(int a_segSize)
{
    int left = (int)(FILE_REGION_SIZE - (m_ofsTop % FILE_REGION_SIZE));

    if(left < a_segSize)
        m_ofsTop += left;

    OFFSET_FT pos = m_ofsTop;
    m_ofsTop += a_segSize;
    return pos;
}

//******************************************************************************
void* TGenericTempFile::ptrFromFileOffset(OFFSET_FT a_ofs)
{
    OFFSET_FT ofsBase = a_ofs & ~FILE_REGION_MASK;

    void* pvBase = m_cache.findRegion(ofsBase);
    if(pvBase == NULL)
    {
        OFFSET_FT ofsEnd = ofsBase + FILE_REGION_SIZE;
        while(ofsEnd > m_file.getSize())
        {
            m_cache.reset();
            m_file.grow();
        }
        pvBase = m_cache.addRegion(ofsBase, m_file);
    }
    int nRelativeOffset = int(a_ofs - ofsBase);

    return (BYTE*)pvBase + nRelativeOffset;
}

//******************************************************************************
void TGenericTempFile::reset()
{
    m_cache.reset();
    m_file.reset();
}

//******************************************************************************
PAGE_INFO_TMP *TTempFile::getPageByOffset(OFFSET_FT a_offset)
{
    PAGE_INFO_TMP *pInfo = (PAGE_INFO_TMP *)ptrFromFileOffset(a_offset);

    assert(pInfo->marker == ID_MARKER);
    assert(pInfo->offset == a_offset);

    return pInfo;
}

//******************************************************************************
PAGE_INFO_TMP *TTempFile::getPageByKey(FPOINTER a_fpPage)
{
    int nHashIdx = (int)(a_fpPage % (FPOINTER)PAGE_INFO_HASH_COUNT);
    
    OFFSET_FT offset = m_hash[nHashIdx];
    while(offset != -1)
    {
        PAGE_INFO_TMP *pInfo = getPageByOffset(offset);
        if(pInfo->fpPage == a_fpPage)
            return pInfo;
        offset = pInfo->next;
    }
    return NULL;
}

//******************************************************************************
bool TGenericTempFile::open()
{
    if(!m_file.isOpen())
    {
        char* pszTmpName = _tempnam(NULL, "TreeBase");
        return m_file.open(pszTmpName);
    }
    return true;
}

//******************************************************************************
bool TTempFile::readPage(FPOINTER a_fpPage, void *a_pvBuff)
{
    try
    {
        PAGE_INFO_TMP *pPageInfo = getPageByKey(a_fpPage);
        if(pPageInfo == NULL)
            return false;

        memcpy(a_pvBuff, pPageInfo->content, PAGESIZE);
    }
    catch(std::exception&)
    {
        return false;
    }
    return true;
}

//******************************************************************************
void TTempFile::writePage(DataPage::PageInfo &a_page)
{
    try
    {
        FPOINTER       fpPage    = a_page.first;
        PAGE_INFO_TMP *pPageInfo = getPageByKey(fpPage);
        
        if(pPageInfo == NULL)
        {
            int       nHashIdx = (int)(fpPage % (FPOINTER)PAGE_INFO_HASH_COUNT);
            OFFSET_FT offset   = m_hash[nHashIdx];
            
            OFFSET_FT newOffset = allocOffset(sizeof(PAGE_INFO_TMP));

            pPageInfo = (PAGE_INFO_TMP *)ptrFromFileOffset(newOffset);
            pPageInfo->marker = ID_MARKER;
            pPageInfo->next   = offset;
            pPageInfo->fpPage = fpPage;
            pPageInfo->offset = newOffset;
            m_hash[nHashIdx] = pPageInfo->offset;
        }
        memcpy(pPageInfo->content, a_page.second, PAGESIZE);
    }
    catch(std::exception&)
    {
    }
}
    
//******************************************************************************
void TTempFile::getPageKeys(std::vector<FPOINTER> &a_arrFP)
{
    try
    {
        for(int ctr = 0; ctr < PAGE_INFO_HASH_COUNT; ctr++)
        {
            OFFSET_FT ofs = m_hash[ctr];
            while(ofs != -1)
            {
                PAGE_INFO_TMP* pInfo = getPageByOffset(ofs);
                a_arrFP.push_back(pInfo->fpPage);
                ofs = pInfo->next;
            }
        }
    }
    catch(std::exception&)
    {
    }
}

//******************************************************************************
void TTempFile::removePage(FPOINTER a_fpPage)
{
    try
    {
        PAGE_INFO_TMP *pPageInfo = getPageByKey(a_fpPage);
        if(pPageInfo)
        {
            pPageInfo->fpPage = FPOINTER(-1);  // invalid
        }
    }
    catch(std::exception&)
    {
    }
}

//******************************************************************************
void TTempFile::reset()
{
    TGenericTempFile::reset();

    for(int ctr = 0; ctr < PAGE_INFO_HASH_COUNT; ctr++)
    {
        m_hash[ctr] = -1;
    }
    m_ofsTop = 0;
}

//******************************************************************************
Util::TCachedPageIterator *TTempFile::getIterator()
{
    return new TTempFilePageIterator(this);
}

//******************************************************************************
bool TTempFile::TTempFilePageIterator::moveFirst()
{
    m_ofsPos = 0;
    return m_ofsPos < m_pTempFile->getTop();
}

//******************************************************************************
bool TTempFile::TTempFilePageIterator::moveNext()
{
    while(m_ofsPos < m_pTempFile->getTop())
    {
        m_ofsPos += sizeof(PAGE_INFO_TMP);

        int left = (int)(FILE_REGION_SIZE - (m_ofsPos % FILE_REGION_SIZE));
        if(left < sizeof(PAGE_INFO_TMP))
            m_ofsPos += left;

        if(m_ofsPos < m_pTempFile->getTop())
        {
            try
            {
                PAGE_INFO_TMP *pPageInfo = m_pTempFile->getPageByOffset(m_ofsPos);
                if(pPageInfo->fpPage != FPOINTER(-1))
                    return true;
            }
            catch(std::exception&)
            {
                m_bError = false;
                return true;
            }
        }
    }
    return false;
}

//********************************************************************************
FPOINTER TTempFile::TTempFilePageIterator::getPage()
{
    if(m_bError)
        return FPOINTER(-1);

    try
    {
        PAGE_INFO_TMP *pPageInfo = m_pTempFile->getPageByOffset(m_ofsPos);
        return pPageInfo->fpPage;
    }
    catch(std::exception&)
    {
        return FPOINTER(-1);
    }
}

//********************************************************************************
bool TTempFile::TTempFilePageIterator::getPageData(void *a_pBuff)
{
    if(m_bError)
        return false;

    try
    {
        PAGE_INFO_TMP *pPageInfo = m_pTempFile->getPageByOffset(m_ofsPos);
        memcpy(a_pBuff, pPageInfo->content, PAGESIZE);
        return true;
    }
    catch(std::exception&)
    {
        m_bError = true;
        return false;
    }
}

//********************************************************************************
template<>
void Util::TCacheEntryOperations::initEntryData<FILE_REGION_DESCRIPTOR>(FILE_REGION_DESCRIPTOR *pEntry)
{
    pEntry->pvMem = NULL;
    pEntry->ofsBase = 0;
};

//********************************************************************************
template<>
void Util::TCacheEntryOperations::freeEntryData<FILE_REGION_DESCRIPTOR>(FILE_REGION_DESCRIPTOR *pEntry)
{
    if(pEntry->pvMem)
        TTempFile::munmap(pEntry->pvMem);
};

