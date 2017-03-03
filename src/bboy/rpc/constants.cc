#include "bboy/rpc/constants.h"

using std::set;

namespace bb {
namespace rpc {

const char* const kMagicNumber = "hrpc";
const char* const kSaslAppName = "bboy";
const char* const kSaslProtoName = "bboy";

// The server supports the TLS flag if there is a TLS certificate available.
// The flag is added during negotiation if this is the case.
//
// NOTE: the TLS_AUTHENTICATION_ONLY flag is dynamically added on both sides
// based on the remote peer's address.
set<RpcFeatureFlag> kSupportedServerRpcFeatureFlags = { APPLICATION_FEATURE_FLAGS };
set<RpcFeatureFlag> kSupportedClientRpcFeatureFlags = { APPLICATION_FEATURE_FLAGS, TLS };

} // namespace rpc
} // namespace kudu
