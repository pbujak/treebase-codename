#if !defined(UNIQUE_BINARY_SEGMENT_H)
#define UNIQUE_BINARY_SEGMENT_H

#include "datamgr.h"

typedef struct _UNIQUE_BINARY_REF
{
    DWORD dwHash;
    WORD  wOrder;
}UNIQUE_BINARY_REF;

class TUniqueBinarySegment : public TSectionSegment
{
    struct BINARY_ENTRY
    {
        int  refCount;
        int  cbSize;
        BYTE data[1];
    };

    class Matcher: public TSectionSegment::ValueMatcher
    {
        void *m_pvData;
        int   m_cbSize;
    public:
        Matcher(DWORD a_dwHash, void *a_pvData, int a_cbSize):
            TSectionSegment::ValueMatcher(a_dwHash),
            m_pvData(a_pvData),
            m_cbSize(a_cbSize)
        {
        }
        bool isMatching(TBITEM_ENTRY *a_pEntry) const;
    };
private:
    DWORD computeHash(void *a_pvData, int a_cbSize);

    static BINARY_ENTRY* getBinaryEntry(TBITEM_ENTRY *a_pEntry);

    static Ref<BINARY_ENTRY> getBinaryEntryRef(Ref<TBITEM_ENTRY> & a_entry);

public:
    TUniqueBinarySegment(void);
public:
    ~TUniqueBinarySegment(void);

    UNIQUE_BINARY_REF AddBinaryBlock(void *a_pvData);
    void *GetBinaryBlock(const UNIQUE_BINARY_REF & a_ref);
    void DeleteBinaryBlock(const UNIQUE_BINARY_REF & a_ref);
    bool ChangeBinaryBlock(const UNIQUE_BINARY_REF & a_ref, void *a_pvData);

    template<typename T>
    static Ref<T> GetDataOfEntryRef(Ref<TBITEM_ENTRY> & a_entry);

    int GetDataOfEntrySize(Ref<TBITEM_ENTRY> & a_entry);
};

#endif // UNIQUE_BINARY_SEGMENT_H