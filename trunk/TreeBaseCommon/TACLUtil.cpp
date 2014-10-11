#include "stdafx.h"
#include "TACLUtil.h"
#include "TreeBaseInt.h"
#include "TreeBasePriv.h"

#define BLK_DELTA 128

TACLSerializer::TACLSerializer(void): 
    m_pvBlock(NULL), m_cbSize(0), m_nOffset(0)
{
}

TACLSerializer::~TACLSerializer(void)
{
}

void TACLSerializer::AddEntry(PSID a_sid, DWORD a_dwAccessRights)
{
    int cbSizeSid = GetLengthSid(a_sid);
    int cbSize = cbSizeSid + sizeof(ACL_BLOCK_ENTRY) - 1;

    while((m_nOffset + cbSize + offsetof(ACL_BLOCK_ENTRY, dwRights)) > m_cbSize)
    {
        m_cbSize += BLK_DELTA;
    }

    if(m_pvBlock == NULL)
    {
        m_pvBlock = SYS_MemAlloc(m_cbSize);
    }
    else if(m_cbSize > SYS_MemSize(m_pvBlock))
    {
        m_pvBlock = SYS_MemReAlloc(m_pvBlock, m_cbSize);
    }

    ACL_BLOCK_ENTRY *pEntry = MAKE_PTR(ACL_BLOCK_ENTRY*, m_pvBlock, m_nOffset);
    pEntry->cbSize = cbSize;
    pEntry->dwRights = a_dwAccessRights;
    CopySid(cbSizeSid, (PSID)pEntry->sid, a_sid);

    m_nOffset += cbSize;
}

void *TACLSerializer::GetResult()
{
    ACL_BLOCK_ENTRY *pEntry = MAKE_PTR(ACL_BLOCK_ENTRY*, m_pvBlock, m_nOffset);
    pEntry->cbSize = 0;    
    return m_pvBlock;
}
