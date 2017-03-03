#ifndef BBOY_RPC_MESSENGER_H_
#define BBOY_RPC_MESSENGER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

#include <boost/optional.hpp>
#include <gtest/gtest_prod.h>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/ref_counted.h"
#include "bboy/security/token.pb.h"
#include "bboy/base/sync/locks.h"
#include "bboy/base/net/sockaddr.h"
#include "bboy/base/status.h"
#include "bboy/base/monotime.h"

// response_callback.h
//
namespace bb {

class Socket;
class ThreadPool;

namespace security {
class TlsContext;
class TokenVerifier;
} // namespace security

namespace rpc {

class AcceptorPool;
class DumpRunningRpcsRequestPB;
class DumpRunningRpcsResponsePB;
class InboundCall;
class Messenger;
class OutboundCall;
class Reactor;
class ReactorThread;
class RpcService;
class RpczStore;

class AcceptorPoolInfo {
 public:
  explicit AcceptorPoolInfo(Sockaddr bind_address)
    : bind_address_(std::move(bind_address)) {}
 
  Sockaddr bind_address() const { return bind_address_; }

 private:
  Sockaddr bind_address_;  
};

// Encryption configuration for RPC connections.
enum class RpcAuthentication {
  DISABLED,
  OPTIONAL,
  REQUIRED,
};

enum class RpcEncryption {
  DISABLED,
  OPTIONAL,
  REQUIRED,
};

class MessengerBuilder {
 public:
  friend class Messenger;
  friend class ReactorThread;

  MessengerBuilder(std::string name);

  MessengerBuilder& set_connection_keepalive_time(const MonoDelta& keepalive);
  MessengerBuilder& set_num_reactors(int num_reactors);
  MessengerBuilder& set_min_negotiation_threads(int min_negotiation_threads);
  MessengerBuilder& set_max_negotiation_threads(int max_negotiation_threads);
  MessengerBuilder& set_coarse_timer_granularity(const MonoDelta& granularity);
  MessengerBuilder& enable_inbound_tls();

  Status Build(std::shared_ptr<Messenger>* messenger);

 private:
  const std::string name_;
  MonoDelta connection_keepalive_time_;
  int num_reactors_;
  int min_negotiation_threads_;
  int max_negotiation_threads_;
  MonoDelta coarse_timer_granularity_;
  // TODO(wqx)
  // metric_entity_;
  bool enable_inbound_tls_;
};

class Messenger {
 public:
  friend class MessengerBuilder;
  friend class Proxy;
  friend class Reactor;
  typedef std::vector<std::shared_ptr<AcceptorPool>> acceptor_vec_t;
  typedef std::unordered_map<std::string, scoped_refptr<RpcService>> RpcServiceMap;

  static const uint64_t UNKNOWN_CALL_ID = 0;

  ~Messenger();

  void Shutdown();
  
  Status AddAcceptorPool(const Sockaddr& accept_addr,
                         std::shared_ptr<AcceptorPool>* pool);

  Status RegisterService(const std::string& service_name,
                         const scoped_refptr<RpcService>& service);
  Status UnregisterService(const std::string& service_name);

  void QueueOutboundCall(const std::shared_ptr<OutboundCall>& call);
  void QueueInboundCall(gscoped_ptr<InboundCall> call);

  void RegisterInboundSocket(Socket* new_socket, const Sockaddr& remote);

  Status DumpRunningRpcs(const DumpRunningRpcsRequestPB& req,
                         DumpRunningRpcsResponsePB* resp);

  void ScheduleOnReactor(const std::function<void(const Status&)>& func,
                         MonoDelta when);

  const security::TlsContext& tls_context() const;
  security::TlsContext* mutable_tls_context();

  const security::TokenVerifier& token_verifier() const;
  security::TokenVerifier* mutable_token_verifier();
  std::shared_ptr<security::TokenVerifier> shared_token_verifier() const;

  boost::optional<security::SignedTokenPB> authn_token() const;
  void set_authn_token(const security::SignedTokenPB& token);

  ThreadPool* negotiation_pool() const;

  RpczStore* rpcz_store();

  int num_reactors() const;

  std::string name() const;

  bool closing() const;

  const scoped_refptr<RpcService> rpc_service(const std::string& service_name) const;

 private:
  FRIEND_TEST(TestRpc, TestConnectionKeepalive);

  explicit Messenger(const MessengerBuilder& builder);

  Reactor* RemoteToReactor(const Sockaddr& remote);
  Status Init();
  void RunTimeoutThread();
  void UpdateCurTime();

  void AllExternalReferencesDropped();

  const std::string name_;

  mutable percpu_rwlock lock_;

  bool closing_;

  RpcAuthentication authentication_;
  RpcEncryption encryption_;

  acceptor_vec_t acceptor_pools_;

  RpcServiceMap rpc_services_;

  std::vector<Reactor*> reactors_;

  gscoped_ptr<ThreadPool> negotiation_pool_;

  std::unique_ptr<security::TlsContext> tls_context_;

  std::shared_ptr<security::TokenVerifier> token_verifier_;

  mutable simple_spinlock authn_token_lock_;
  boost::optional<security::SignedTokenPB> authn_token_;

  std::unique_ptr<RpczStore> rpcz_store_;
  
  std::shared_ptr<Messenger> retain_self_;

  DISALLOW_COPY_AND_ASSIGN(Messenger);
};

} // namespace rpc
} // namespace bb
#endif // BBOY_RPC_MESSENGER_H_
