#ifndef _SESSION_H_
#define _SESSION_H_

#include "Util.h"
#include "Storage.h"
#include <atlsecurity.h>

namespace Security
{
    namespace Facade
    {
        class ATL::CSid;
    }
};

namespace Session
{

struct UserInfo
{
    Util::Uuid uid;
    SEGID_F    junctionSegId;
};

void getUserInfo(ATL::CSid &a_sid, UserInfo &a_info);
void releaseUserInfo(ATL::CSid &a_sid);
void getComputerInfo(LPCSTR a_pszComputer, SEGID_F &a_info);
void releaseComputerInfo(LPCSTR a_pszComputer);

}

#endif // _SESSION_H_