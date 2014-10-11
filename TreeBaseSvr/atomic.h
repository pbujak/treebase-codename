// atomic.h

#if !defined(_ATOMIC_H_)
#define _ATOMIC_H_

template<class T>
class TAtomic32
{
protected:
    volatile T m_data;
public:
    TAtomic32() {};

    inline T operator=(T a_Src)
    {
        InterlockedExchange((LPLONG)&m_data, (LONG)a_Src);
        return m_data;
    };

    inline BOOL operator==(T a_Src)
    {
        return m_data==a_Src;
    }

    inline operator T()
    {
        return m_data;
    };

    inline TAtomic32(T a_Src)
    {
        m_data = a_Src;
    };
};

template<class T>
class TIncDecAtomic32: public TAtomic32<T>
{
public:
    inline TIncDecAtomic32(){};
    inline TIncDecAtomic32(T a_Src)
    {
        m_data = a_Src;
    };
    inline T operator++(int) // X++
    {
        T val = m_data;
        InterlockedIncrement((LPLONG)&m_data);
        return val;
    };
    inline T operator++() // ++X
    {
        InterlockedIncrement((LPLONG)&m_data);
        return m_data;
    };
    inline T operator--(int) // X--
    {
        T val = m_data;
        InterlockedDecrement((LPLONG)&m_data);
        return val;
    };
    inline T operator--() // --X
    {
        InterlockedDecrement((LPLONG)&m_data);
        return m_data;
    };
};


#endif //_ATOMIC_H_