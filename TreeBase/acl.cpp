#include "stdafx.h"
#include "acl.h"
#include "TreeBase.h"
#include "tbcore.h"
#include "Shared\TAbstractThreadLocal.h"
#include "TACLUtil.h"

static TCriticalSection                       S_cs;
static TThreadLocal<TAccessControlList::Slot> S_lastSlot;

std::set<TAccessControlList*> TAccessControlList::s_setAcl;

//***************************************************************************************************
TLockAcl::TLockAcl()
{
    S_cs.Enter();
}

//***************************************************************************************************
TLockAcl::~TLockAcl()
{
    S_cs.Leave();
}

//***************************************************************************************************
TAccessControlList::Slot::Slot(PSID a_sid, DWORD a_dwAccessRights)
{
    int cbSize = GetLengthSid(a_sid);
    m_pEntry = (TB_ACL_ENTRY*)malloc(sizeof(TB_ACL_ENTRY) + cbSize - 1);
    if(m_pEntry == NULL)
        return;

    m_pEntry->refCount = 1;
    m_pEntry->dwAccessRights = a_dwAccessRights;
    CopySid(cbSize, (PSID)m_pEntry->sid, a_sid);
}

//***************************************************************************************************
TAccessControlList::Slot::Slot(const TAccessControlList::Slot& a_source): 
    m_pEntry(a_source.m_pEntry)
{
    if(m_pEntry)
        m_pEntry->refCount++;
}

//***************************************************************************************************
TAccessControlList::Slot& TAccessControlList::Slot::operator=(const TAccessControlList::Slot& a_source)
{
    m_pEntry = a_source.m_pEntry;
    if(m_pEntry)
        m_pEntry->refCount++;

    return *this;
}

//***************************************************************************************************
TAccessControlList::Slot::~Slot()
{
    if(m_pEntry && (--m_pEntry->refCount) == 0)
        free(m_pEntry);
}

//***************************************************************************************************
TAccessControlList::TAccessControlList(void *a_pvBlock)
{
    s_setAcl.insert(this);

    int cbSize = _msize(a_pvBlock);
    m_pvBlock = malloc(cbSize);
    memcpy(m_pvBlock, a_pvBlock, cbSize);

    ACL_BLOCK_ENTRY *pEntry = (ACL_BLOCK_ENTRY *)a_pvBlock;

    while(pEntry->cbSize)
    {
        std::vector<TAccessControlList::Slot>::iterator it = getSlot((PSID)pEntry->sid, true);
        it->m_pEntry->dwAccessRights = pEntry->dwRights;

        pEntry = MAKE_PTR(ACL_BLOCK_ENTRY*, pEntry, pEntry->cbSize);
    }
}

//***************************************************************************************************
TAccessControlList::~TAccessControlList()
{
    if(m_pvBlock)
        free(m_pvBlock);

    s_setAcl.erase(this);
}

//***************************************************************************************************
std::vector<TAccessControlList::Slot>::iterator TAccessControlList::getSlot(PSID a_sid, bool a_bCreate)
{
    for(std::vector<TAccessControlList::Slot>::iterator it = m_slots.begin(); it != m_slots.end(); ++it)
    {
        if(EqualSid(a_sid, (PSID)it->m_pEntry->sid))
            return it;
    }

    if(!a_bCreate)
        return m_slots.end();

    m_slots.push_back(Slot(a_sid, 0));
    return m_slots.end() - 1;
}

//***************************************************************************************************
TAccessControlList *TAccessControlList::FromHandle(HTBACL a_hAcl)
{
    std::set<TAccessControlList*>::const_iterator it = s_setAcl.find((TAccessControlList*)a_hAcl);
    if(it == s_setAcl.end())
        return NULL;

    return *it;
}

//***************************************************************************************************
void TAccessControlList::resetBlock()
{
    if(m_pvBlock)
    {
        free(m_pvBlock);
        m_pvBlock = NULL;
    }
}

//***************************************************************************************************
BOOL TAccessControlList::acceptNewSid(PSID a_sid)
{
    if(!IsValidSid(a_sid))
        return FALSE;

    //--------------------------------------------------------------------------------------
    char         szName[128] = {0};
    char         szDomain[128] = {0};
    SID_NAME_USE sidNameUse = SidTypeUnknown;
    DWORD        cbSize = 128;

    if(LookupAccountSid(NULL, a_sid, szName, &cbSize, szDomain, &cbSize, &sidNameUse) && 
       sidNameUse == SidTypeUser)
    {
        return TRUE;
    };

    //--------------------------------------------------------------------------------------
    const SID_IDENTIFIER_AUTHORITY NT_Authority = SECURITY_NT_AUTHORITY;

    PSID_IDENTIFIER_AUTHORITY pAuth = GetSidIdentifierAuthority(a_sid);
    if(pAuth && memcmp(pAuth, &NT_Authority, sizeof(SID_IDENTIFIER_AUTHORITY)) == 0)
    {
        int nCount = *GetSidSubAuthorityCount(a_sid);

        for(int i = 0; i < nCount; ++i)
        {
            int nSubAuth = *GetSidSubAuthority(a_sid, i);

            if(nSubAuth == SECURITY_LOCAL_SERVICE_RID ||
               nSubAuth == SECURITY_NETWORK_SERVICE_RID ||
               nSubAuth == SECURITY_LOCAL_SYSTEM_RID)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//***************************************************************************************************
BOOL TAccessControlList::SetAccessRights(PSID a_sid, DWORD a_dwAccessRights)
{
    if(!acceptNewSid(a_sid))
    {
        TASK_SetErrorInfo(TRERR_INVALID_USERID, NULL, NULL);
        return FALSE;
    };

    resetBlock();

    std::vector<TAccessControlList::Slot>::iterator it = getSlot(a_sid, true);
    it->m_pEntry->dwAccessRights = a_dwAccessRights;
    return TRUE;
}

//***************************************************************************************************
BOOL TAccessControlList::ResetAccessRights(PSID a_sid)
{
    if(!IsValidSid(a_sid))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }

    resetBlock();

    std::vector<TAccessControlList::Slot>::iterator it = getSlot(a_sid);
    if(it != m_slots.end())
        m_slots.erase(it);

    return TRUE;
}

//***************************************************************************************************
BOOL TAccessControlList::GetAccessRights(PSID a_sid, DWORD *a_pdwAccessRights)
{
    if(!IsValidSid(a_sid) || IsBadWritePtr(a_pdwAccessRights, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }

    std::vector<TAccessControlList::Slot>::iterator it = getSlot(a_sid);

    if(it != m_slots.end())
    {
        *a_pdwAccessRights = it->m_pEntry->dwAccessRights;
        return TRUE;
    }
    return FALSE;
}

//***************************************************************************************************
BOOL TAccessControlList::GetEntry(DWORD a_dwIndex, PSID *a_psid, DWORD *a_pdwAccessRights)
{
    if(a_dwIndex >= m_slots.size() ||
       IsBadWritePtr(a_psid, sizeof(PSID)) ||
       IsBadWritePtr(a_pdwAccessRights, sizeof(DWORD)))
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }

    if(!S_lastSlot.IsValid())
        return FALSE;

    S_lastSlot = m_slots[a_dwIndex].clone();
    
    *a_pdwAccessRights = S_lastSlot.Value().m_pEntry->dwAccessRights;
    *a_psid = (PSID)S_lastSlot.Value().m_pEntry->sid;

    return TRUE;
}

//***************************************************************************************************
void* TAccessControlList::GetBlock()
{
    if(m_pvBlock)
        return m_pvBlock;

    TACLSerializer aclSerializer;

    for(std::vector<Slot>::const_iterator it = m_slots.begin();
        it != m_slots.end();
        ++it)
    {
        aclSerializer.AddEntry((PSID)it->m_pEntry->sid, it->m_pEntry->dwAccessRights);
    }
    m_pvBlock = aclSerializer.GetResult();
    return m_pvBlock;
}

//***************************************************************************************************
BOOL ACL_SerializeAcl(HTBACL a_hAcl, TCallParams &a_inParams, int a_svpCode)
{
    if(a_hAcl == NULL)
        return TRUE;

    TAccessControlList *pList = TAccessControlList::FromHandle(a_hAcl);
    if(pList == NULL)
    {
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL);
        return FALSE;
    }

    void *pvBlk = pList->GetBlock();

    if(pvBlk)
        a_inParams.SetBufferPointer(a_svpCode, pvBlk, _msize(pvBlk));

    return TRUE;
}

//***************************************************************************************************
HTBACL ACL_DeserializeAcl(TCallParams &a_outParams, int a_svpCode)
{
    void *pvBuff = NULL;
    long cbSize = 0;

    if(!a_outParams.GetBuffer(a_svpCode, &pvBuff, cbSize))
        return NULL;

    return (HTBACL)(new TAccessControlList(pvBuff));
}
