#include "StdAfx.h"

#if defined(UNIT_TEST)
#define MOCK_SEGMENT
#endif

#include "..\..\TUniqueBinarySegment.h"
#include "..\..\SecurityManager.h"
#include "..\..\TSecurityAttributes.h"

static TUniqueBinarySegment           mockSecurityStore;
static Security::TTaskSecurityContext context;

Security::SECURITY_BASE_INFORMATION Security::Manager::SystemSecurity = { Util::Uuid(), Util::Uuid(), TBACCESS_READ, 0, 0 };
Security::SECURITY_BASE_INFORMATION Security::Manager::TempSecurity   = { Util::Uuid(), Util::Uuid(), TBACCESS_ALL, 0, 0 };
Security::SECURITY_BASE_INFORMATION Security::Manager::LabelSecurity  = { Util::Uuid(), Util::Uuid(), TBACCESS_ALL, 0, 0 };

//***********************************************************************
static BOOL init()
{
    mockSecurityStore.Resize(1);
    return TRUE;
}

IMPLEMENT_INIT(init)

//***********************************************************************
Security::REF Security::Manager::AddSecurity(int a_ndbId, const Security::TSecurityAttributes &a_attributes)
{
    return mockSecurityStore.AddBinaryBlock(a_attributes.getBlock());
}

//***********************************************************************
void Security::Manager::DeleteSecurity(int a_ndbId, const REF& a_refSecurity)
{
    mockSecurityStore.DeleteBinaryBlock(a_refSecurity);
}

//***********************************************************************
bool Security::Manager::GetSecurity(int a_ndbId, const REF& a_refSecurity, Security::TSecurityAttributes &a_attributes)
{
    Security::SECURITY_ATTRIBUTES_BLOCK *pBlock = (SECURITY_ATTRIBUTES_BLOCK *)mockSecurityStore.GetBinaryBlock(a_refSecurity);

    if(pBlock == NULL)
        return false;

    a_attributes.attach(pBlock);
    return true;
}

//***********************************************************************
bool Security::Manager::SetSecurity(int a_ndbId, 
                                    const Util::Uuid& a_protectionDomain, 
                                    TBSECURITY_OPERATION::Enum operation, 
                                    TSecurityAttributes &a_attributes)
{
    ASSERT(!"Not implemented");
    return false;
}

//***********************************************************************
bool Security::Manager::AccessCheck(int a_ndbId, const REF& a_refSecurity, 
                                    int a_minimalDesired, int a_maximalDesired, int &a_granted)
{
    Security::TSecurityAttributes attributes;

    if(!Security::Manager::GetSecurity(a_ndbId, a_refSecurity, attributes))
        return false;

    return attributes.accessCheck(&context, a_minimalDesired, a_maximalDesired, a_granted);
}

//***********************************************************************
void Security::Manager::MergeSecurity(TBSECURITY_OPERATION::Enum operation, 
                                      TSecurityAttributes &a_target,
                                      const TSecurityAttributes &a_source)
{
    switch(operation)
    {
        case TBSECURITY_OPERATION::eREPLACE:
        {
            Util::Uuid owner = a_target.getBase()->owner;
            Util::Uuid protectionDomain = a_target.getBase()->protectionDomain;

            a_target = a_source;
            a_target.getBase()->owner = owner;
            a_target.getBase()->protectionDomain = protectionDomain;

            break;
        }
        case TBSECURITY_OPERATION::eADD:
        {
            a_target += a_source;
            break;
        }
        case TBSECURITY_OPERATION::eREMOVE:
        {
            a_target -= a_source;
            break;
        }
    }
}

//***********************************************************************
Security::TTaskSecurityContext* Security::Manager::GetTaskContext()
{
    return &context;
}