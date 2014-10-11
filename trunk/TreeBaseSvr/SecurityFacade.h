#ifndef _SECURITY_FACADE_
#define _SECURITY_FACADE_

#include "Util.h"
#include <atlsecurity.h>

class TCallParams;

namespace Security
{
class TSecurityAttributes;

namespace Facade
{
    extern const Util::Uuid EmergencyUID;

    Util::Uuid getInternalUID(ATL::CSid &a_sid);
    ATL::CSid  getSystemUID(const Util::Uuid &a_uid);

    bool deserializeSecurity(TCallParams &a_inParams, std::auto_ptr<TSecurityAttributes> &a_outSecAttrs);
    bool serializeSecurity(TCallParams &a_outParams, std::auto_ptr<TSecurityAttributes> &a_inSecAttrs);

} // namespace Facade

} // namespace Security

#endif // _SECURITY_FACADE_