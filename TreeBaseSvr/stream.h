#ifndef _STREAM_H
#define _STREAM_H
#include "TreeBase.h"

namespace Stream
{

    class TInputStream
    {
    public:
        virtual ~TInputStream() {};
        virtual void* read(DWORD a_cbSize) = 0;
        virtual void close() = 0;
    };

    class TOutputStream
    {
    public:
        virtual ~TOutputStream() {};
        virtual DWORD write(void *a_pvBuff, DWORD a_cbSize) = 0;
        virtual bool close() = 0;
    };

    bool close(TInputStream *a_pStream);
    bool close(TOutputStream *a_pStream);

    template<typename T>
    T* fromHandle(HTBHANDLE a_handle);
};

#endif // _STREAM_H