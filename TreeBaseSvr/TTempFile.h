/* 
 * File:   TTempFile.h
 * Author: pabu
 *
 * Created on 28 January 2013, 11:24
 */

#ifndef TTEMPFILE_H
#define	TTEMPFILE_H

#include "TPageCache.h"
#include "TMemoryPool.h"
#include "TGenericCache.h"
#include "Util.h"
#include <list>
#include <cstdio>
#include <cstddef>

#define PAGE_INFO_HASH_COUNT 71

namespace Storage{

typedef __off_t OFFSET_FT;    

typedef struct _PAGE_INFO_TMP{
    WORD      marker;
    FPOINTER  fpPage;
    OFFSET_FT offset, next;
    BYTE      content[PAGESIZE];
}PAGE_INFO_TMP;
    
typedef struct _FILE_REGION_DESCRIPTOR
{
    OFFSET_FT ofsBase;
    void     *pvMem;
}FILE_REGION_DESCRIPTOR;

inline bool operator==(const PAGE_INFO_TMP &a, const PAGE_INFO_TMP &b)
{
    return a.fpPage == b.fpPage && a.offset == b.offset; 
}

class TGenericTempFile
{
    //===================================================================
    //inline TTempFile(const TTempFile& orig){};
    class MMFile
    {
        TGenericHandle<HANDLE> m_hFile, m_hFileMapping;
        OFFSET_FT              m_size;
    public:
        ~MMFile();

        bool open(const char *a_pszPath);  
        inline bool isOpen()
        {
            return m_hFile;
        }
        inline OFFSET_FT getSize()
        {
            return m_size;
        }
        void grow();
        void* mmap(OFFSET_FT a_ofsBase);
        static void munmap(void *a_pvAddr);
        void reset();
    };
    //===================================================================
    class KeyMaker
    {
        OFFSET_FT m_ofsBase;
    public:
        inline KeyMaker(OFFSET_FT a_ofsBase): m_ofsBase(a_ofsBase)
        {
        };
        inline KeyMaker(FILE_REGION_DESCRIPTOR *a_pDesc): 
            m_ofsBase(a_pDesc->ofsBase)
        {
        }

        inline DWORD getHash() const
        {
            return (DWORD)m_ofsBase;
        }

        inline bool isMatching(FILE_REGION_DESCRIPTOR *a_pDesc) const
        {
            return m_ofsBase == a_pDesc->ofsBase;
        };

        inline OFFSET_FT base() const
        {
            return m_ofsBase;
        }

        inline void fillNewEntry(FILE_REGION_DESCRIPTOR *a_pEntry) const
        {
            a_pEntry->ofsBase = m_ofsBase;
        };

    };
    //===================================================================
    enum ESizes
    {
        eCacheHashCount = 17,
        eCacheEntryCount = 5
    };
    //===================================================================
    typedef Util::TGenericCache<
        FILE_REGION_DESCRIPTOR, 
        KeyMaker, 
        eCacheEntryCount, 
        eCacheHashCount,
        Util::TSingleThreaded
    > CacheBase;
    //===================================================================
    class Cache : public CacheBase
    {
    public:
        void* findRegion(OFFSET_FT a_ofsBase);
        void* addRegion(OFFSET_FT a_ofsBase, MMFile &a_file);
    };
    //===================================================================
private:
    Cache   m_cache;
    MMFile  m_file;
protected:
    void    reset();
public:
    inline TGenericTempFile()
    {}
    bool open();
    void* ptrFromFileOffset(OFFSET_FT a_ofs);
    
    inline static void munmap(void *a_pvSeg)
    {
        TGenericTempFile::MMFile::munmap(a_pvSeg);
    }
};

class TTempFile : public TGenericTempFile
{
    class TTempFilePageIterator : public Util::TCachedPageIterator
    {
        bool       m_bError;
        TTempFile *m_pTempFile;
        OFFSET_FT  m_ofsPos;
    public:

        inline TTempFilePageIterator(TTempFile *a_pTempFile): 
            m_pTempFile(a_pTempFile), m_bError(false) {}

        virtual bool moveFirst();
        virtual bool moveNext();
        virtual FPOINTER getPage();
        virtual bool getPageData(void *a_pBuff);
    };


public:
    TTempFile();
    virtual ~TTempFile();
private:
    Util::TMemoryPool         m_pageInfoPool;
    std::list<PAGE_INFO_TMP*> m_listPageInfo;
    OFFSET_FT                 m_hash[PAGE_INFO_HASH_COUNT];
    OFFSET_FT                 m_ofsTop;
private:
    PAGE_INFO_TMP *getPageByOffset(OFFSET_FT a_offset);
    PAGE_INFO_TMP *getPageByKey(FPOINTER a_fpPage);
    OFFSET_FT      allocOffset(int a_segSize);
protected:
    inline OFFSET_FT getTop()
    {
        return m_ofsTop;
    }
public:
    void reset();
    bool readPage(FPOINTER a_fpPage, void *a_pvBuff);
    void writePage(DataPage::PageInfo &a_page);
    void getPageKeys(std::vector<FPOINTER> &a_arrFP);
    void removePage(FPOINTER a_fpPage);
    Util::TCachedPageIterator *getIterator();
};


};
#endif	/* TTEMPFILE_H */

