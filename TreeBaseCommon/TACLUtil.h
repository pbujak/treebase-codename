#ifndef _ACLUTIL_H_
#define _ACLUTIL_H_

#include "TreeBaseCommon.h"

typedef struct _ACL_BLOCK_ENTRY
{
    WORD  cbSize;
    DWORD dwRights;
    BYTE  sid[1];
}ACL_BLOCK_ENTRY;

class __TBCOMMON_EXPORT__ TACLSerializer
{
    void *m_pvBlock;
    int   m_nOffset, m_cbSize;
public:
    TACLSerializer(void);

    void AddEntry(PSID a_sid, DWORD a_dwAccessRights);

    void *GetResult();
public:
    ~TACLSerializer(void);
};

#endif // _ACLUTIL_H_