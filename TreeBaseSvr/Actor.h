#pragma once
#include "TTask.h"
#include "syncobj.h"
#include <queue>
#include <map>

namespace Actor
{
    template<class T>
    struct ArgInfo
    {
        typedef T Type;
    };

    template<>
    struct ArgInfo<CString>
    {
        typedef const char* Type;
    };

    class TMessageQueueTask;

    //===============================================
    class TAbstractMessage
    {
    public:
        virtual void handle() = 0;
    public:
        virtual ~TAbstractMessage(){};
    };

    //===============================================
    class TAbstractMessageFactory
    {
    public:
        virtual TAbstractMessage* create() = 0;
    };

    //===============================================
    template<class D>
    class TMessageFactory0: public TAbstractMessageFactory
    {
        typedef void (D::*PFX)();

        PFX m_pfx;
        D  *m_pDest;
    public:
        inline TMessageFactory0(D *a_pDest, PFX a_pfx)
        {
            m_pfx = a_pfx;
            m_pDest = a_pDest;
        }

        TAbstractMessage* create()
        {
            TMessage0<D> *pMsg = new TMessage0<D>();
            pMsg->SetTarget(m_pDest, m_pfx);
            return pMsg;
        };
    };

    //===============================================
    template<class D, class A1>
    class TMessageFactory1: public TAbstractMessageFactory
    {
        typedef typename ArgInfo<A1>::Type ArgA1;

        typedef void (D::*PFX)(ArgA1);

        PFX m_pfx;
        D  *m_pDest;
        A1  m_a1;
    public:
        inline TMessageFactory1(D *a_pDest, PFX a_pfx, ArgA1 a_a1)
        {
            m_pfx = a_pfx;
            m_pDest = a_pDest;
            m_a1 = a_a1;
        }

        TAbstractMessage* create()
        {
            TMessage1<D, A1> *pMsg = new TMessage1<D, A1>(m_a1);
            pMsg->SetTarget(m_pDest, m_pfx);
            return pMsg;
        };
    };

    //===============================================
    template<class D>
    class TMessage0: public TAbstractMessage
    {
        typedef void (D::*PFX)();

        PFX m_pfx;
        D  *m_pDest;
    public:
        inline TMessage0(){};

        inline void SetTarget(D *a_pDest, PFX a_pfx)
        {
            m_pfx = a_pfx;
            m_pDest = a_pDest;
        }
        inline void Send(D *a_pDest, PFX a_pfx)
        {
            SetTarget(a_pDest, a_pfx);
            a_pDest->Send(this);
        }

        virtual void handle()
        {
            (m_pDest->*m_pfx)();
        }
    };
    //===============================================
    template<class D, class A1>
    class TMessage1: public TAbstractMessage
    {
        typedef typename ArgInfo<A1>::Type ArgA1;

        typedef void (D::*PFX)(ArgA1);

        PFX m_pfx;
        D  *m_pDest;
        A1  m_a1;
    public:
        inline TMessage1(ArgA1 a_a1): m_a1(a_a1) {};

        inline void SetTarget(D *a_pDest, PFX a_pfx)
        {
            m_pfx = a_pfx;
            m_pDest = a_pDest;
        }
        inline void Send(D *a_pDest, PFX a_pfx)
        {
            SetTarget(a_pDest, a_pfx);
            a_pDest->Send(this);
        }
        virtual void handle()
        {
            (m_pDest->*m_pfx)(m_a1);
        }
    };
    //===============================================
    template<class D, class A1, class A2>
    class TMessage2: public TAbstractMessage
    {
        typedef typename ArgInfo<A1>::Type ArgA1;
        typedef typename ArgInfo<A2>::Type ArgA2;

        typedef void (D::*PFX)(ArgA1, ArgA2);

        PFX m_pfx;
        D  *m_pDest;
        A1  m_a1;
        A2  m_a2;
    public:
        inline TMessage2(ArgA1 a_a1, ArgA2 a_a2): m_a1(a_a1), m_a2(a_a2) {};

        inline void SetTarget(D *a_pDest, PFX a_pfx)
        {
            m_pfx = a_pfx;
            m_pDest = a_pDest;
        }
        inline void Send(D *a_pDest, PFX a_pfx)
        {
            SetTarget(a_pDest, a_pfx);
            a_pDest->Send(this);
        }
        virtual void handle()
        {
            (m_pDest->*m_pfx)(m_a1, m_a2);
        }
    };

    //===============================================
    class TMessageQueueTask: public TTask
    {
        enum {eEventMax = 16};

        std::queue<TAbstractMessage*> m_messageQueue;
        TCriticalSection              m_messageQueue_CS;
        int                           m_nTimerRate;
        bool                          m_bWaitForMessage, m_bStop;
        THandle                       m_messageQueue_hEvent;
        std::map<HANDLE, TAbstractMessageFactory*> m_mapExtraHandles;
    private:
        HANDLE WaitForObjects(TCSLock &a_lock);
    public:
        TMessageQueueTask(int a_nTimerRate = INFINITE);
    protected:
        TAbstractMessage* GetMessage(int a_nTimeout);

        inline void Stop()
        {
            m_bStop = true;
        }

        inline void MapExtraHandle(HANDLE a_handle, TAbstractMessageFactory* a_pMsgFactory)
        {
            m_mapExtraHandles[a_handle] = a_pMsgFactory;
        }
    public:
        ~TMessageQueueTask(void);
        void Send(TAbstractMessage* a_pMsg);

        virtual void OnRun();
        virtual void OnTimer(DWORD a_dwTicks) {};
    };

};

