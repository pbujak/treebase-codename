#ifndef _ACL_H_
#define _ACL_H_

#include <vector>
#include <set>
#include "TreeBase.h"

#define ACL_IMPLEMENT_API(ret_err, hacl, call) \
    TAccessControlList *pList = TAccessControlList::FromHandle(hacl); \
    if(pList == NULL) \
    { \
        TASK_SetErrorInfo(TRERR_INVALID_PARAMETER, NULL, NULL); \
        return ret_err; \
    } \
    return pList->call;

typedef struct _TB_ACL_ENTRY
{
    DWORD refCount; 
    DWORD dwAccessRights;
    BYTE  sid[1];
}TB_ACL_ENTRY;

class TLockAcl
{
public:
    TLockAcl();
    ~TLockAcl();
};

class TAccessControlList
{
public:
    class Slot
    {
    public:
        TB_ACL_ENTRY *m_pEntry;

    public:
        inline Slot(): m_pEntry(NULL) {};
        Slot(PSID a_sid, DWORD a_dwAccessRights);
        Slot(const Slot& a_source);
        ~Slot();
        Slot& operator=(const Slot& a_source);
        inline Slot clone()
        {
            return Slot((PSID)m_pEntry->sid, m_pEntry->dwAccessRights);
        }
    };

private:
    static std::set<TAccessControlList*> s_setAcl;

    void *m_pvBlock;

    std::vector<Slot> m_slots;

    std::vector<Slot>::iterator getSlot(PSID a_sid, bool a_bCreate = false);
public:
    inline TAccessControlList(): m_pvBlock(NULL)
    {
        s_setAcl.insert(this);
    }
    TAccessControlList(void *a_pvBlock);
    ~TAccessControlList();
    inline BOOL Destroy()
    {
        delete this;
        return TRUE;
    }
    static TAccessControlList *FromHandle(HTBACL a_hAcl);

    BOOL ResetAccessRights(PSID a_sid);
    BOOL SetAccessRights(PSID a_sid, DWORD a_dwAccessRights);
    BOOL GetAccessRights(PSID a_sid, DWORD *a_pdwAccessRights);
    inline int GetCount()
    {
        return m_slots.size();
    }
    BOOL GetEntry(DWORD a_dwIndex, PSID *a_psid, DWORD *a_pdwAccessRights);
    void* GetBlock();
private:
    BOOL acceptNewSid(PSID a_sid);
    void resetBlock();
};

class TCallParams;

BOOL ACL_SerializeAcl(HTBACL a_hAcl, TCallParams &a_inParams, int a_svpCode);
HTBACL ACL_DeserializeAcl(TCallParams &a_inParams, int a_svpCode);


#endif // _ACL_H_