#include "stdafx.h"
#include "TSecurityAttributes.h"
#include "TTask.h"
#include <vector>
#include <assert.h>
#include <algorithm>

using namespace Security;

namespace{

    class IsUser
    {
        Util::Uuid m_user;
    public:
        IsUser(const Util::Uuid & _user): m_user(_user)
        {
        }

        bool operator()(const BW_LIST_ENTRY &a_entry)
        {
            return a_entry.user == m_user;
        }
    };

}

//================================================================================
class TSecurityAttributes::Data
{
public:
    SECURITY_BASE_INFORMATION m_base;

    std::vector<BW_LIST_ENTRY> m_whiteList, m_blackList;
};

//********************************************************************************
TSecurityAttributes::TSecurityAttributes(void):
    m_pBlock(NULL), m_pData(NULL), m_ownerOverride(false)
{
}

//********************************************************************************
TSecurityAttributes::TSecurityAttributes(SECURITY_BASE_INFORMATION *a_pBase):
    m_pBlock(NULL), m_pData(NULL), m_ownerOverride(false)
{
    if(a_pBase->protectionDomain == Util::EmptyUuid)
        a_pBase->protectionDomain = Util::UuidGenerate();

    m_pBlock = (SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(sizeof(SECURITY_ATTRIBUTES_BLOCK) - sizeof(BW_LIST_ENTRY));
    memset(m_pBlock, 0, sizeof(SECURITY_ATTRIBUTES_BLOCK) - sizeof(BW_LIST_ENTRY));
    m_pBlock->base = *a_pBase;
}

//********************************************************************************
TSecurityAttributes::TSecurityAttributes(
        DWORD a_allAccessRights,
        DWORD a_adminAccessRights,
        DWORD a_ownerAccessRights):
    m_pBlock(NULL), m_pData(new Data()), m_ownerOverride(false)
{
    m_pData->m_base.allAccessRights = a_allAccessRights;
    m_pData->m_base.adminAccessRights = a_adminAccessRights;
    m_pData->m_base.ownerAccessRights = a_ownerAccessRights;
}

//********************************************************************************
TSecurityAttributes::TSecurityAttributes(SECURITY_ATTRIBUTES_BLOCK *a_pBlock):
    m_pBlock(a_pBlock), m_pData(NULL), m_ownerOverride(false)
{
}


//********************************************************************************
TSecurityAttributes::~TSecurityAttributes(void)
{
    if(m_pData)
    {
        delete m_pData;
    }
    else if(m_pBlock)
    {
        TASK_MemFree(m_pBlock);
    }
}

//********************************************************************************
void TSecurityAttributes::makeEditableFormat()
{
    if(m_pData)
        return;

    m_pData = new Data();
    if(m_pBlock)
    {
        m_pData->m_base = m_pBlock->base;

        int size = m_pBlock->blackListEnd - m_pBlock->blackListStart;
        if(size > 0)
        {
            m_pData->m_blackList.resize(size);
            memcpy(&m_pData->m_blackList[0], &m_pBlock->blackWhiteList[m_pBlock->blackListStart], size * sizeof(BW_LIST_ENTRY));
        }

        size = m_pBlock->whiteListEnd - m_pBlock->whiteListStart;
        if(size > 0)
        {
            m_pData->m_whiteList.resize(size);
            memcpy(&m_pData->m_whiteList[0], &m_pBlock->blackWhiteList[m_pBlock->whiteListStart], size * sizeof(BW_LIST_ENTRY));
        }

        TASK_MemFree(m_pBlock);
        m_pBlock = NULL;
    }
}

//********************************************************************************
void TSecurityAttributes::makeBlockFormat() const
{
    if(m_pBlock)
        return;

    int cbSize = sizeof(SECURITY_ATTRIBUTES_BLOCK) + 
                (m_pData->m_whiteList.size() + m_pData->m_blackList.size() - 1) * sizeof(BW_LIST_ENTRY);

    m_pBlock = (SECURITY_ATTRIBUTES_BLOCK*)TASK_MemAlloc(cbSize);

    m_pBlock->base = m_pData->m_base;

    int iIndexBW = 0;

    int count = m_pData->m_blackList.size();
    m_pBlock->blackListStart = iIndexBW;
    if(count)
    {
        memcpy(&m_pBlock->blackWhiteList[iIndexBW], &m_pData->m_blackList[0], count * sizeof(BW_LIST_ENTRY));
        iIndexBW += count;
    }
    m_pBlock->blackListEnd = iIndexBW;

    count = m_pData->m_whiteList.size();
    m_pBlock->whiteListStart = iIndexBW;
    if(count)
    {
        memcpy(&m_pBlock->blackWhiteList[iIndexBW], &m_pData->m_whiteList[0], count * sizeof(BW_LIST_ENTRY));
        iIndexBW += count;
    }
    m_pBlock->whiteListEnd = iIndexBW;

    _release_ptr(m_pData);
}

//********************************************************************************
SECURITY_BASE_INFORMATION *TSecurityAttributes::getBase() const
{
    if(m_pData)
    {
        return &m_pData->m_base;
    }
    else if(m_pBlock)
    {
        return &m_pBlock->base;
    }
    else
    {
        m_pBlock = (SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(sizeof(SECURITY_ATTRIBUTES_BLOCK) - sizeof(BW_LIST_ENTRY));
        memset(m_pBlock, 0, sizeof(SECURITY_ATTRIBUTES_BLOCK) - sizeof(BW_LIST_ENTRY));
        return &m_pBlock->base;
    }
    return NULL;
}

//********************************************************************************
void TSecurityAttributes::attach(SECURITY_ATTRIBUTES_BLOCK *a_pBlock)
{
    assert(!m_pData && !m_pBlock);

    m_pBlock = a_pBlock;
}

//********************************************************************************
const BW_LIST_ENTRY* TSecurityAttributes::beginWhiteList() const
{
    if(m_pData)
    {
        if(m_pData->m_whiteList.empty())
            return NULL;
        return &m_pData->m_whiteList[0];
    }
    else
        return &m_pBlock->blackWhiteList[m_pBlock->whiteListStart];
}

//********************************************************************************
const BW_LIST_ENTRY* TSecurityAttributes::endWhiteList() const
{
    if(m_pData)
    {
        if(m_pData->m_whiteList.empty())
            return NULL;
        return &m_pData->m_whiteList[0] + m_pData->m_whiteList.size();
    }
    else
        return &m_pBlock->blackWhiteList[m_pBlock->whiteListEnd];
}


//********************************************************************************
const BW_LIST_ENTRY* TSecurityAttributes::beginBlackList() const
{
    if(m_pData)
    {
        if(m_pData->m_blackList.empty())
            return NULL;

        return &m_pData->m_blackList[0];
    }
    else
        return &m_pBlock->blackWhiteList[m_pBlock->blackListStart];
}

//********************************************************************************
const BW_LIST_ENTRY* TSecurityAttributes::endBlackList() const
{
    if(m_pData)
    {
        if(m_pData->m_blackList.empty())
            return NULL;

        return &m_pData->m_blackList[0] + m_pData->m_blackList.size();
    }
    else
        return &m_pBlock->blackWhiteList[m_pBlock->blackListEnd];
}

//********************************************************************************
std::vector<BW_LIST_ENTRY>& TSecurityAttributes::getBlackList()
{
    makeEditableFormat();
    return m_pData->m_blackList;
}

//********************************************************************************
std::vector<BW_LIST_ENTRY>& TSecurityAttributes::getWhiteList()
{
    makeEditableFormat();
    return m_pData->m_whiteList;
}

//********************************************************************************
bool TSecurityAttributes::accessCheck(
    const TTaskSecurityContext *a_pContext, 
    int a_minimalDesired,
    int a_maximalDesired,
    int &a_granted) const
{
    assert( (a_minimalDesired & a_maximalDesired) == a_minimalDesired );

    if(a_pContext->privilege == TTaskSecurityContext::PrivilegeLevel::eSystem)
    {
        a_granted = a_maximalDesired;
        return true;
    }
    else if(a_pContext->privilege == TTaskSecurityContext::PrivilegeLevel::eEmergency)
    {
        a_minimalDesired &= TBACCESS_READ;
        a_maximalDesired &= TBACCESS_READ;

        if(a_minimalDesired == 0)
            return false;
    }

    SECURITY_BASE_INFORMATION *pBase = getBase();

    int access = pBase->allAccessRights;

    if(a_pContext->privilege == TTaskSecurityContext::PrivilegeLevel::eAdmin)
    {
        access |= pBase->adminAccessRights;
    }

    if(a_pContext->user == pBase->owner)
    {
        access |= pBase->ownerAccessRights;
    }

    access &= a_maximalDesired;

    if(access != a_maximalDesired)
    {
        for(const BW_LIST_ENTRY *it = beginWhiteList(); it != endWhiteList(); ++it)
        {
            if(it->user == a_pContext->user)
            {
                access |= it->accessRights;
                break;
            }
        }
    }

    access &= a_maximalDesired;

    for(const BW_LIST_ENTRY *it = beginBlackList(); it != endBlackList(); ++it)
    {
        if(it->user == a_pContext->user)
        {
            access &= ~it->accessRights;
            break;
        }
    }

    a_granted = access;

    return ((access & a_minimalDesired) == a_minimalDesired);
}

//********************************************************************************
void TSecurityAttributes::addList(std::vector<BW_LIST_ENTRY>& a_list, const BW_LIST_ENTRY* a_pBegin, const BW_LIST_ENTRY* a_pEnd)
{
    for(const BW_LIST_ENTRY* it = a_pBegin; it != a_pEnd; it++)
    {
        std::vector<BW_LIST_ENTRY>::iterator itFound = std::find_if(a_list.begin(), a_list.end(), IsUser(it->user));

        if(itFound != a_list.end())
        {
            itFound->accessRights |= it->accessRights;
        }
        else
            a_list.push_back(*it);
    }
}

//********************************************************************************
void TSecurityAttributes::substList(std::vector<BW_LIST_ENTRY>& a_list, const BW_LIST_ENTRY* a_pBegin, const BW_LIST_ENTRY* a_pEnd)
{
    for(const BW_LIST_ENTRY* it = a_pBegin; it != a_pEnd; it++)
    {
        std::vector<BW_LIST_ENTRY>::iterator itFound = std::find_if(a_list.begin(), a_list.end(), IsUser(it->user));

        if(itFound != a_list.end())
        {
            itFound->accessRights &= ~it->accessRights;
            if(itFound->accessRights == 0)
                a_list.erase(itFound);
        }
    }
}

//********************************************************************************
TSecurityAttributes& TSecurityAttributes::operator+=(const TSecurityAttributes& a_rhs)
{
    SECURITY_BASE_INFORMATION *pBase = getBase();
    pBase->ownerAccessRights |= a_rhs.getBase()->ownerAccessRights;
    pBase->adminAccessRights |= a_rhs.getBase()->adminAccessRights;
    pBase->allAccessRights   |= a_rhs.getBase()->allAccessRights;

    addList(getBlackList(), a_rhs.beginBlackList(), a_rhs.endBlackList());
    addList(getWhiteList(), a_rhs.beginWhiteList(), a_rhs.endWhiteList());

    return *this;
}

//********************************************************************************
TSecurityAttributes& TSecurityAttributes::operator-=(const TSecurityAttributes& a_rhs)
{
    SECURITY_BASE_INFORMATION *pBase = getBase();
    pBase->ownerAccessRights &= ~(a_rhs.getBase()->ownerAccessRights);
    pBase->adminAccessRights &= ~(a_rhs.getBase()->adminAccessRights);
    pBase->allAccessRights   &= ~(a_rhs.getBase()->allAccessRights);

    substList(getBlackList(), a_rhs.beginBlackList(), a_rhs.endBlackList());
    substList(getWhiteList(), a_rhs.beginWhiteList(), a_rhs.endWhiteList());

    return *this;
}

//********************************************************************************
TSecurityAttributes& TSecurityAttributes::operator=(const TSecurityAttributes& a_rhs)
{
    makeBlockFormat();

    if(m_pBlock)
    {
        TASK_MemFree(m_pBlock);
        m_pBlock = NULL;
    }

    SECURITY_ATTRIBUTES_BLOCK *pBlock = a_rhs.getBlock();

    int cbSize = TASK_MemSize(pBlock);

    m_pBlock = (SECURITY_ATTRIBUTES_BLOCK *)TASK_MemAlloc(cbSize);
    memcpy(m_pBlock, pBlock, cbSize);

    return *this;
}

