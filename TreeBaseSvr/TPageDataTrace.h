#pragma once

#include <map>
#include <set>

namespace Debug
{

class TPageDataTrace
{
    std::map<FPOINTER, long> m_mapPageToCRC;
    std::set<FPOINTER>       m_discarded;

    TPageDataTrace(void);

    long computeChecksum(void *a_pvData);
public:
    ~TPageDataTrace(void);

    void updatePageCRC(FPOINTER a_fpPage, void* a_pvData);
    void checkPageCRC(FPOINTER a_fpPage, void* a_pvData);
    void setPageDiscard(FPOINTER a_fpPage, bool a_bDiscarded);

    static TPageDataTrace* getInstance();
    static void suppress();
};

}