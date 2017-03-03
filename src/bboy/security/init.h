#pragma once

#include <string>

#include <boost/optional/optional_fwd.hpp>

namespace bb {

class RWMutex;
class Status;

namespace security {

// Initializes Kerberos for a server. In particular, this processes
// the '--keytab_file' command line flag.
Status InitKerberosForServer();

// Returns the process lock 'kerberos_reinit_lock'
// This lock is acquired in write mode while the ticket is being renewed, and
// acquired in read mode before using the SASL library which might require a ticket.
RWMutex* KerberosReinitLock();

// Return the full principal (user/host@REALM) that the server has used to
// log in from the keytab.
//
// If the server has not logged in from a keytab, returns boost::none.
boost::optional<std::string> GetLoggedInPrincipalFromKeytab();

// Same, but returns the mapped short username.
boost::optional<std::string> GetLoggedInUsernameFromKeytab();

// Canonicalize the given principal name by adding '@DEFAULT_REALM' in the case that
// the principal has no realm.
//
// TODO(todd): move to kerberos_util.h in the later patch in this series (the file doesn't
// exist yet, and trying to avoid rebase pain).
Status CanonicalizeKrb5Principal(std::string* principal);

// Map the given Kerberos principal 'principal' to a short username (i.e. with no realm or
// host component).
//
// This respects the "auth-to-local" mappings from the system krb5.conf. However, if no such
// mapping can be found, we fall back to simply taking the first component of the principal.
//
// TODO(todd): move to kerberos_util.h in the later patch in this series (the file doesn't
// exist yet, and trying to avoid rebase pain).
Status MapPrincipalToLocalName(const std::string& principal, std::string* local_name);

} // namespace security
} // namespace bb
