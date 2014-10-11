#ifndef _UTIL_H_
#define _UTIL_H_

#include <set>

class TSection;

template<bool B>
struct StaticAssert
{
};

template<>
struct StaticAssert<true>
{
    inline static void doAssert(){};
};

#define STATIC_ASSERT(x) StaticAssert<(x)>::doAssert();

template<typename _Handle> 
class TGenericHandle
{
    _Handle m_handle;
public:
    inline TGenericHandle(): m_handle(NULL) {};
    inline TGenericHandle(_Handle a_handle): m_handle(a_handle){};

    inline operator _Handle()
    {
        return m_handle;
    }
	inline operator bool()
	{
		return m_handle != NULL;
	}
    TGenericHandle& operator=(_Handle a_handle);

    ~TGenericHandle();
};

typedef TGenericHandle<HANDLE> THandle;

namespace Util{

template<class T>
class TTaskAutoPtr
{
    mutable T* m_ptr;
public:
    inline TTaskAutoPtr(T* a_ptr = NULL): m_ptr(a_ptr) {}

    inline TTaskAutoPtr(const TTaskAutoPtr &a_src)
    {
        m_ptr = a_src.m_ptr;
        a_src.m_ptr = NULL;
    }

    inline operator T*()
    {
        return m_ptr;
    }
    inline T* operator->()
    {
        return m_ptr;
    }
    inline TTaskAutoPtr& operator=(T *a_ptr)
    {
        if(m_ptr)
            TASK_MemFree(m_ptr);
        m_ptr = a_ptr;
        return *this;
    }
    inline TTaskAutoPtr& operator=(const TTaskAutoPtr &a_src)
    {
        if(m_ptr)
            TASK_MemFree(m_ptr);
        m_ptr = a_src.m_ptr;
        a_src.m_ptr = NULL;
        return *this;
    }
    inline bool realloc(int a_nSize)
    {
        if(m_ptr)
        {
            T *ptr = (T*)TASK_MemReAlloc(m_ptr, a_nSize);
            if(ptr == NULL)
                return false;
            m_ptr = ptr;
            return TRUE;
        }
        return FALSE;
    }

    inline T* ptr()
    {
        return m_ptr;
    }

    inline T* detach()
    {
        T *p = m_ptr;
        m_ptr = NULL;
        return p;
    }

    ~TTaskAutoPtr()
    {
        if(m_ptr)
            TASK_MemFree(m_ptr);
    }
};

class TSectionPtr
{
    TSection *m_pSection;

    inline TSectionPtr(const TSectionPtr&){};
public:
    inline TSection* operator->()
    {
        return m_pSection;
    }
    operator TSection*()
    {
        return m_pSection;
    }
    TSectionPtr(TSection *a_pSection = NULL): m_pSection(a_pSection)
    {
    }
    ~TSectionPtr();

    TSectionPtr& operator=(TSection *a_pSection);
};

template<class T>
class TState
{
    typedef void (T::*PFN)();

    PFN  m_pfnEntry, m_pfnExit;
    T   *m_pObj;
public:
    TState(T* a_pObj, PFN a_pfnEntry, PFN a_pfnExit): 
        m_pObj(a_pObj), m_pfnEntry(a_pfnEntry), m_pfnExit(a_pfnExit)
    {
        (a_pObj->*a_pfnEntry)();
    };

    ~TState()
    {
        (m_pObj->*m_pfnExit)();
    }
};

template<class T>
class TScopeExitCall
{
    typedef void (T::*PFN)();

    PFN  m_pfnEntry, m_pfnExit;
    T   *m_pObj;
public:
    TScopeExitCall(T* a_pObj, PFN a_pfnExit): 
        m_pObj(a_pObj), m_pfnExit(a_pfnExit)
    {
    };

    ~TScopeExitCall()
    {
        (m_pObj->*m_pfnExit)();
    }
};

class TCommandLineInfo
{
    inline TCommandLineInfo() {};
public:
    enum ECommand {eDefault = 0, eDebug, eInstall, eUnInstall, eStart, eStop, 
                   eInvalid = -1};

    ECommand m_eCommand;

    TCommandLineInfo(LPCSTR a_pszArgs);
};

template<typename T>
T GetEnvironmentString(const char *a_pszEnvVarName);

template<typename T>
class TEnvSetting
{
    T m_setting;
public:
    TEnvSetting(const char *a_pszEnvVarName)
    {
        m_setting = GetEnvironmentString<T>(a_pszEnvVarName);
    }

    inline operator T()
    {
        return m_setting;
    }
};

//========================================
class TComputeCRC
{
/* Table of CRCs of all 8-bit messages. */
   static DWORD S_crc_table[256];

   /* Flag: has the table been computed? Initially false. */
   static BOOL S_crc_table_computed;

   void make_crc_table(void);
   unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);
 
public:
    __inline TComputeCRC(){};
    unsigned long  crc(LPVOID a_pvBuff, int len);
};

typedef BOOL (*INIT_PROC)();

//========================================
class TInitialize
{
public:
    TInitialize(INIT_PROC a_pProc)
    {
       (*a_pProc)();
    }
};

//========================================
class TCachedPageIterator
{
public:
    virtual ~TCachedPageIterator() {};

    virtual bool moveFirst() = 0;
    virtual bool moveNext() = 0;
    virtual FPOINTER getPage() = 0;
    virtual bool getPageData(void *a_pBuff) = 0;
};

//========================================
template<class T> std::set<T*> &getObjectSet();

template<class T> void releaseObject(T *p);

//========================================
template<class T>
struct TTaskObjectSet
{
    typedef std::set<T*> SetObjects;
    typedef typename SetObjects::iterator SetObjectsIter;

    SetObjects &m_setObjects;
    SetObjectsIter m_it;

public:
    TTaskObjectSet(): m_setObjects(getObjectSet<T>())
    {
    }

    inline bool exists(T *p)
    {
        m_it = m_setObjects.find(p);
        return m_it != m_setObjects.end();
    }

    inline T* insert(T *p)
    {
        if(p) m_setObjects.insert(p);
        return p;
    }

    void removeAll()
    {
        SetObjectsIter it; 
        for(it = m_setObjects.begin(); it != m_setObjects.end(); it++)
        {
            T *p = *it;
            releaseObject(p);
        }
        m_setObjects.clear();
    }

    inline void removeLastFound()
    {
        T *p = *m_it;
        releaseObject(p);
        m_setObjects.erase(m_it);
    }

    template<typename HANDLE>
    T* fromHandle(HANDLE a_handle)
    {
        T* p = (T*)a_handle;

        if(m_setObjects.find(p) == m_setObjects.end())
            return NULL;
        
        return p;
    }
};

const char* message(LPCSTR a_pszFormat, ...);

struct Uuid
{
    Uuid();
    Uuid(const CString &_string);
    DWORD data1;
    WORD  data2;
    WORD  data3;
    WORD  data4;
    BYTE  data5[6];
};

CString UuidToString(const Uuid &_uuid);
CString UuidToSectionName(const Uuid &_uuid);
Uuid    UuidFromString(const CString &_string);
Uuid    UuidGenerate();

extern Uuid EmptyUuid;

} // namespace Util

bool operator==(const Util::Uuid &uid1, const Util::Uuid &uid2);

template<typename T>
inline bool operator!=(const T &a, const T &b)
{
    return !(a == b);
}

#endif // _UTIL_H_