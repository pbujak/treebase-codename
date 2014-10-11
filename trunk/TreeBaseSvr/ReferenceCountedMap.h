#ifndef _REFERENCE_COUNTED_MAP_H_
#define _REFERENCE_COUNTED_MAP_H_

#include <afxtempl.h>
#include <utility>

namespace Util
{

//===================================================================
template<typename Key, typename RefKey, typename Value>
class CReferenceCountedMap
{
    typedef std::pair<Value, int> RefCountedValue;

    CMap<Key, RefKey, RefCountedValue*, RefCountedValue*> m_map;

public:
    //***************************************************************
    bool getValue(RefKey a_key, Value &a_value)
    {
        RefCountedValue *pRefCountedValue;

        if(m_map.Lookup(a_key, pRefCountedValue))
        {
            pRefCountedValue->second++;
            a_value = pRefCountedValue->first;
            return true;
        }
        return false;
    }

    //***************************************************************
    void releaseValue(RefKey a_key)
    {
        RefCountedValue *pRefCountedValue;

        if(!m_map.Lookup(a_key, pRefCountedValue))
            return;

        if((--pRefCountedValue->second) == 0)
        {
            m_map.RemoveKey(a_key);
            delete pRefCountedValue;
        }
    }

    //***************************************************************
    void addValue(RefKey a_key, Value &a_value)
    {
        RefCountedValue* pRefCountedValue = new RefCountedValue();

        pRefCountedValue->first = a_value;
        pRefCountedValue->second = 1;

        m_map[a_key] = pRefCountedValue;
    }

    //***************************************************************
    ~CReferenceCountedMap()
    {
        POSITION pos = m_map.GetStartPosition();
        RefCountedValue *pRefCountedValue;
        Key key;

        while(pos)
        {
            m_map.GetNextAssoc(pos, key, pRefCountedValue);
            delete pRefCountedValue;
        }
    }
};

}

#endif // _REFERENCE_COUNTED_MAP_H_