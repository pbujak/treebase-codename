#ifndef _SECURITY_MANAGER_H_
#define _SECURITY_MANAGER_H_

#include "TUniqueBinarySegment.h"
#include "TSecurityAttributes.h"

namespace Security
{

class TSecurityAttributes;
struct TTaskSecurityContext;

typedef _UNIQUE_BINARY_REF REF;

namespace Manager
{

extern Security::SECURITY_BASE_INFORMATION  AdminSecurity, SystemSecurity, TempSecurity, LabelSecurity, BlobSecurity, AuthSecurity;

TTaskSecurityContext* GetTaskContext();
REF AddSecurity(int a_ndbId, const TSecurityAttributes &a_attributes);
void DeleteSecurity(int a_ndbId, const REF& a_refSecurity);
bool GetSecurity(int a_ndbId, const REF& a_refSecurity, TSecurityAttributes &a_attributes);
bool SetSecurity(int a_ndbId, 
                 const Util::Uuid& a_protectionDomain, 
                 TBSECURITY_OPERATION::Enum operation, 
                 TSecurityAttributes &a_attributes);

bool AccessCheck(int a_ndbId, const REF& a_refSecurity, 
                 int a_minimalDesired, int a_maximalDesired, int &a_granted);
bool ValidateNewSecurity(const TSecurityAttributes &a_attributes);
void InheritSecurity(TSecurityAttributes &a_attributes);

void MergeSecurity(TBSECURITY_OPERATION::Enum operation, 
                   TSecurityAttributes &a_target,
                   const TSecurityAttributes &a_source);
 

} // namespace Manager

class TPrivilegedScope
{
    TTaskSecurityContext m_savedContext;

public:
    TPrivilegedScope();
    ~TPrivilegedScope();
};

} // namespace Security

#endif // _SECURITY_MANAGER_H_