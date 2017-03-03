#include "bboy/rpc/messenger.h"

#include "bboy/rpc/acceptor_pool.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <list>
#include <mutex>
#include <set>
#include <string>

#include "bboy/base/net/socket.h"
#include "bboy/base/net/sockaddr.h"

DEFINE_string(rpc_authentication, "optional",
  "Whether to require RPC connections to authenticate. Must be one "
  "of 'disabled', 'optional', or 'required'. If 'optional', "
  "authentication will be used when the remote end supports it. If "
  "'required', connections which are not able to authenticate "
  "(because the remote end lacks support) are rejected. Secure "
  "clusters should use 'required'.");

DEFINE_string(rpc_encryption, "optional",
  "Whether to require RPC connections to be encrypted. Must be one "
  "of 'disabled', 'optional', or 'required'. If 'optional', "
  "encryption will be used when the remote end supports it. If "
  "'required', connections which are not able to use encryption "
  "(because the remote end lacks support) are rejected. If 'disabled', "
  "encryption will not be used, and RPC authentication "
  "(--rpc_authentication) must also be disabled as well. "
  "Secure clusters should use 'required'.");

DEFINE_string(rpc_certificate_file, "",
  "Path to a PEM encoded X509 certificate to use for securing RPC "
  "connections with SSL/TLS. If set, '--rpc_private_key_file' and "
  "'--rpc_ca_certificate_file' must be set as well.");

DEFINE_string(rpc_private_key_file, "",
  "Path to a PEM encoded private key paired with the certificate "
  "from '--rpc_certificate_file'");

DEFINE_string(rpc_ca_certificate_file, "",
  "Path to the PEM encoded X509 certificate of the trusted external "
  "certificate authority. The provided certificate should be the root "
  "issuer of the certificate passed in '--rpc_certificate_file'.");

DEFINE_int32(rpc_default_keepalive_time_ms, 65000,
  "If an RPC connection from a client is idle for this amount of time, the server "
  "will disconnect the client.");
  
DECLARE_string(keytab_file);

namespace bb {
namespace rpc {

MessengerBuilder::MessengerBuilder(std::string name)
  : name_(std::move(name)),
    connection_keepalive_time_(MonoDelta::FromMilliseconds(FLAGS_rpc_default_keepalive_time_ms)),
    num_reactors_(4),
    min_negotiation_threads_(0),
    max_negotiation_threads_(0),
    coarse_timer_granularity_(MonoDelta::FromMilliseconds(100)),
    enable_inbound_tls_(false) {
}

MessengerBuilder& MessengerBuilder::set_connection_keepalive_time(const MonoDelta& keepalive) {
  connection_keepalive_time_ = keepalive;
  return *this;
}

MessengerBuilder& MessengerBuilder::set_num_reactors(int num_reactors) {
  num_reactors_ = num_reactors;
  return *this;
}
  
MessengerBuilder& MessengerBuilder::set_min_negotiation_threads(int min_negotiation_threads) {
  min_negotiation_threads_ = min_negotiation_threads;
  return *this;
}
  
MessengerBuilder& MessengerBuilder::set_max_negotiation_threads(int max_negotiation_threads) {
  max_negotiation_threads_ = max_negotiation_threads;
  return *this;
}
  
MessengerBuilder& MessengerBuilder::set_coarse_timer_granularity(const MonoDelta &granularity) {
  coarse_timer_granularity_ = granularity;
  return *this;
}
  
MessengerBuilder& MessengerBuilder::enable_inbound_tls() {
  enable_inbound_tls_ = true;
  return *this;
}

//TODO(wqx):
//Build

/////////////////// Messenger
Status Messenger::AddAcceptorPool(const Sockaddr& accept_addr,
                                  std::shared_ptr<AcceptorPool>* pool) {
  if (!FLAGS_keytab_file.empty()) {
  }
  
  Socket sock;
  RETURN_NOT_OK(sock.Init(0));
  RETURN_NOT_OK(sock.SetReuseAddr(true));
  RETURN_NOT_OK(sock.Bind(accept_addr));
  Sockaddr remote;
  RETURN_NOT_OK(sock.GetSocketAddress(&remote));
  std::shared_ptr<AcceptorPool> acceptor_pool(new AcceptorPool(this, &sock, remote));

  std::lock_guard<percpu_rwlock> guard(lock_);
  acceptor_pools_.push_back(acceptor_pool);
  *pool = acceptor_pool;
  return Status::OK();
}


} // namespace rpc
} // namespace bb
