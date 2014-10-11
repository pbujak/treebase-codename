#include "stdafx.h"
#include "Util.h"
#include "Shared\TAbstractThreadLocal.h"
#include <winsvc.h>
#include "stream.h"
#include "section.h"
#include <iostream>
#include <sstream>

#pragma comment(lib, "Rpcrt4.lib")

static const char* UUID_STRING_FORMAT = "%8.8X-%4.4X-%4.4X-%4.4X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X";

//================================================================
template class TGenericHandle<HANDLE>;
template class TGenericHandle<SC_HANDLE>;

//************************************************************************
template<>
void Util::releaseObject<TSection>(TSection *a_pSection)
{
    a_pSection->Release();
}
//************************************************************************
template<>
void Util::releaseObject<Stream::TInputStream >(Stream::TInputStream *a_pStream)
{
    delete a_pStream;
}
//************************************************************************
template<>
void Util::releaseObject<Stream::TOutputStream>(Stream::TOutputStream *a_pStream)
{
    delete a_pStream;
}


static TThreadLocal<std::set< TSection*> > s_setSections;
static TThreadLocal<std::set< Stream::TInputStream*> > s_setInputStreams;
static TThreadLocal<std::set< Stream::TOutputStream*> > s_setOutputStreams;

//************************************************************************
template<>
std::set<TSection*> &Util::getObjectSet<TSection>()
{
    return s_setSections.Value();
}
//************************************************************************
template<>
std::set<Stream::TInputStream*> &Util::getObjectSet<Stream::TInputStream>()
{
    return s_setInputStreams.Value();
}
//************************************************************************
template<>
std::set<Stream::TOutputStream*> &Util::getObjectSet<Stream::TOutputStream>()
{
    return s_setOutputStreams.Value();
}

//================================================================
template struct Util::TTaskObjectSet<TSection>;
template struct Util::TTaskObjectSet<Stream::TInputStream>;
template struct Util::TTaskObjectSet<Stream::TOutputStream>;

static TThreadLocal<CString> S_strMessage;

//************************************************************************
inline void _ReleaseHandle(HANDLE a_handle)
{
	CloseHandle(a_handle);
}

//************************************************************************
inline void _ReleaseHandle(SC_HANDLE a_handle)
{
	CloseServiceHandle(a_handle);
}

//************************************************************************
template<typename _Handle>
TGenericHandle<_Handle>::~TGenericHandle()
{
    if(m_handle) _ReleaseHandle(m_handle);
}

//************************************************************************
template<typename _Handle>
TGenericHandle<_Handle>& TGenericHandle<_Handle>::operator=(_Handle a_handle)
{
    if(m_handle)
        _ReleaseHandle(m_handle);
    m_handle = a_handle;
    return *this;
}

//************************************************************************
const char* Util::message(LPCSTR a_pszFormat, ...)
{
    va_list vl;
    va_start(vl, a_pszFormat);

    CString str;
    str.FormatV(a_pszFormat, vl);
    S_strMessage.Value() = str;
    va_end(vl);

    return S_strMessage.Value();
}

//************************************************************************
Util::TCommandLineInfo::TCommandLineInfo(LPCSTR a_pszArgs)
{
    int     nPos = 0;
    CString strArgs(a_pszArgs);

    CString strTok = strArgs.Tokenize(" \t", nPos);
    if(strTok.IsEmpty())
    {
        m_eCommand = eDefault;
        return;
    }

    m_eCommand = eInvalid;
    if(strTok[0] == '-' || strTok[0] == '/')
    {
        strTok.Delete(0);

        if(strTok.CompareNoCase("debug") == 0)
        {
            m_eCommand = eDebug;
        }
        else if(strTok.CollateNoCase("install") == 0)
        {
            m_eCommand = eInstall;
        }
        else if(strTok.CollateNoCase("uninstall") == 0)
        {
            m_eCommand = eUnInstall;
        }
        else if(strTok.CollateNoCase("start") == 0)
        {
            m_eCommand = eStart;
        }
        else if(strTok.CollateNoCase("stop") == 0)
        {
            m_eCommand = eStop;
        }
    }

    strTok = strArgs.Tokenize(" \t", nPos);
    if(!strTok.IsEmpty())
        m_eCommand = eInvalid;
}

//******************************************************************
template<>
bool Util::GetEnvironmentString<bool>(const char *a_pszEnvVarName)
{
    char szBuff[10] = {0};
    ::GetEnvironmentVariable(a_pszEnvVarName, szBuff, sizeof(szBuff)-1);
    
    return (stricmp(szBuff,"YES") == 0);
}

BOOL Util::TComputeCRC::S_crc_table_computed = FALSE;
DWORD Util::TComputeCRC::S_crc_table[256]    = {0};

//===========================================================
/* Make the table for a fast CRC. */
void Util::TComputeCRC::make_crc_table(void)
{
     DWORD c;
     int n, k;

     for (n = 0; n < 256; n++) 
     {
       c = (DWORD) n;
       for (k = 0; k < 8; k++) 
       {
         if (c & 1)
           c = 0xedb88320L ^ (c >> 1);
         else
           c = c >> 1;
       }
       S_crc_table[n] = c;
     }
     S_crc_table_computed = 1;
}
   
   /* Update a running CRC with the bytes buf[0..len-1]--the CRC
      should be initialized to all 1's, and the transmitted value
      is the 1's complement of the final running CRC (see the
      crc() routine below)). */
   
unsigned long Util::TComputeCRC::update_crc(unsigned long crc, unsigned char *buf,
                                      int len)
{
     unsigned long c = crc;
     int           n = 0;
     if (!S_crc_table_computed)
       make_crc_table();
     for (n = 0; n < len; n++) 
     {
       c = S_crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
     }
     return c;
}
   
/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long Util::TComputeCRC::crc(LPVOID a_pvBuf, int len)
{
    BYTE *buf = (BYTE*)a_pvBuf;
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

template<typename T>
static void pushUuidValue(CString &_str, T _value)
{
    CString tmp;

    for(int i = 0; i < sizeof(T)*2; ++i)
    {
        int ch = _value & 0xF;

        if(ch < 10) 
        {
            ch += '0';
        }
        else
            ch = ch + 'A' - 10;

        tmp.Insert(0, (char)ch);

        _value >>= 4;
    }

    _str += tmp;
}

template<typename T>
static T nextUuidToken(LPCSTR &_input)
{
    T value = 0;

    if(*_input == '-')
        _input++;

    for(int i = 0; i < sizeof(T)*2; ++i)
    {
        char ch = *(_input++);
        if(isdigit(ch))
        {
            ch -= '0';
        }
        else if(ch >= 'A' && ch <= 'F')
        {
            ch = ch - 'A' + 10;
        }
        else
            ch = ch - 'a' + 10;

        value = (value << 4) | ch;
    }
    return value;
}

Util::Uuid Util::EmptyUuid;

Util::Uuid::Uuid()
{
    memset(this, 0, sizeof(Uuid));
}

Util::Uuid::Uuid(const CString &_string)
{
    *this = UuidFromString(_string);
}

CString Util::UuidToString(const Util::Uuid &_uuid)
{
    CString result;

    pushUuidValue(result, _uuid.data1);
    result += '-';

    pushUuidValue(result, _uuid.data2);
    result += '-';

    pushUuidValue(result, _uuid.data3);
    result += '-';

    pushUuidValue(result, _uuid.data4);
    result += '-';

    for(int i = 0; i < sizeof(_uuid.data5); ++i)
    {
        pushUuidValue(result, _uuid.data5[i]);
    }

    return result;
}

CString Util::UuidToSectionName(const Util::Uuid &_uuid)
{
    CString result = UuidToString(_uuid);

    result.Insert(0, "UUID.");

    return result;
}


Util::Uuid Util::UuidFromString(const CString &_string)
{
    Uuid uuid;

    LPCSTR pos = _string;

    uuid.data1 = nextUuidToken<DWORD>(pos);
    uuid.data2 = nextUuidToken<WORD>(pos);
    uuid.data3 = nextUuidToken<WORD>(pos);
    uuid.data4 = nextUuidToken<WORD>(pos);

    for(int i = 0; i < sizeof(uuid.data5); ++i)
    {
        uuid.data5[i] = nextUuidToken<BYTE>(pos);
    }

    return uuid;
}

Util::Uuid Util::UuidGenerate()
{
    GUID winGUID;

    ::UuidCreate(&winGUID);

    Uuid uuid;
    uuid.data1 = winGUID.Data1;
    uuid.data2 = winGUID.Data2;
    uuid.data3 = winGUID.Data3;
    uuid.data4 = *reinterpret_cast<WORD*>(&winGUID.Data4[0]);
    memcpy(&uuid.data5, &winGUID.Data4[2], 6);
    return uuid;
}

bool operator==(const Util::Uuid &uid1, const Util::Uuid &uid2)
{
    return memcmp(&uid1, &uid2, sizeof(Util::Uuid)) == 0;
}

