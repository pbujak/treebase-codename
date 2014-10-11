#ifndef _SECURITY_ATTRIBUTES_H_
#define _SECURITY_ATTRIBUTES_H_

#include "Util.h"
#include <vector>

namespace Security
{

struct TTaskSecurityContext;


typedef struct _SECURITY_BASE_INFORMATION
{
    Util::Uuid  owner;
    Util::Uuid  protectionDomain;
    DWORD       allAccessRights;
    DWORD       adminAccessRights;
    DWORD       ownerAccessRights;
}SECURITY_BASE_INFORMATION;
    
typedef struct _BW_LIST_ENTRY
{
    Util::Uuid user;
    DWORD      accessRights;
}BW_LIST_ENTRY;


typedef struct _SECURITY_ATTRIBUTES_BLOCK
{
    SECURITY_BASE_INFORMATION base;
    BYTE           whiteListStart;
    BYTE           whiteListEnd;
    BYTE           blackListStart;
    BYTE           blackListEnd;
    BW_LIST_ENTRY  blackWhiteList[1];
}SECURITY_ATTRIBUTES_BLOCK;


class TSecurityAttributes
{
    class Data;

    mutable Data *m_pData;
    mutable SECURITY_ATTRIBUTES_BLOCK *m_pBlock;
    bool m_ownerOverride;
private:
    void makeEditableFormat();
    void makeBlockFormat() const;
    void addList(std::vector<BW_LIST_ENTRY>& list, const BW_LIST_ENTRY* a_pBegin, const BW_LIST_ENTRY* a_pEnd);
    void substList(std::vector<BW_LIST_ENTRY>& list, const BW_LIST_ENTRY* a_pBegin, const BW_LIST_ENTRY* a_pEnd);

public:
    TSecurityAttributes(void);
    TSecurityAttributes(
        DWORD a_allAccessRights,
        DWORD a_adminAccessRights,
        DWORD a_ownerAccessRights);
    TSecurityAttributes(SECURITY_BASE_INFORMATION *a_pBase);
    TSecurityAttributes(SECURITY_ATTRIBUTES_BLOCK *a_pBlock);
    void attach(SECURITY_ATTRIBUTES_BLOCK *a_pBlock);

    const BW_LIST_ENTRY* beginWhiteList() const;
    const BW_LIST_ENTRY* endWhiteList() const;
    const BW_LIST_ENTRY* beginBlackList() const;
    const BW_LIST_ENTRY* endBlackList() const;

    std::vector<BW_LIST_ENTRY>& getBlackList();
    std::vector<BW_LIST_ENTRY>& getWhiteList();

    SECURITY_BASE_INFORMATION *getBase() const;
    inline SECURITY_ATTRIBUTES_BLOCK* getBlock() const
    {
        makeBlockFormat();
        return m_pBlock;
    }
    bool accessCheck(const TTaskSecurityContext *a_pContext, 
                     int a_minimalDesired, int a_maximalDesired, int &a_granted) const;

    inline bool getOwnerOverride() const 
    {
        return m_ownerOverride; 
    }
    inline void setOwnerOverride(bool a_ownerOverride) 
    {
        m_ownerOverride = a_ownerOverride;
    }

    TSecurityAttributes& operator=(const TSecurityAttributes& a_rhs);
    TSecurityAttributes& operator+=(const TSecurityAttributes& a_rhs);
    TSecurityAttributes& operator-=(const TSecurityAttributes& a_rhs);

public :
    ~TSecurityAttributes(void);
};


struct TTaskSecurityContext
{
    struct PrivilegeLevel
    {
        enum Enum
        {
            eSystem = 0,
            eAdmin = 1,
            eUser = 2,
            eEmergency = 3
        };
    };
    PrivilegeLevel::Enum privilege;
    Util::Uuid           user;
};

} // namespace Security

#endif // _SECURITY_ATTRIBUTES_H_