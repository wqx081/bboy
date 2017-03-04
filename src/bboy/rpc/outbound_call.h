#pragma once

#include <set>
#include <string>
#include <glog/logging.h>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/macros.h"

#include "bboy/rpc/constants.h"
#include "bboy/rpc/rpc_header.pb.h"
#include "bboy/rpc/remote_method.h"
#include "bboy/rpc/response_callback.h"
#include "bboy/rpc/transfer.h"
#include "bboy/rpc/user_credentials.h"

#include "bboy/base/sync/locks.h"
#include "bboy/base/monotime.h"
#include "bboy/base/slice.h"
#include "bboy/base/status.h"
#include "bboy/base/net/sockaddr.h"

namespace google {
namespace protobuf {
class Message;
} // namespace protobuf
} // namespace google

namespace bb {
namespace rpc {

class CallResponse;
class Connection;
class DumpRunningRpcsRequestPB;
class InboundTransfer;
class RpcCallInProcessPB;
class RpcController;

class ConnectionId {
 public:
  ConnectionId();
  ConnectionId(const ConnectionId& other);
  ConnectionId(const Sockaddr& remote, UserCredentials user_credentials);

  void set_remote(const Sockaddr& remote);
  const Sockaddr& remote() const;

  void set_user_credentials(UserCredentials user_credentials);
  const UserCredentials& user_credentials() const;
  UserCredentials* mutable_user_credentials();

  void CopyFrom(const ConnectionId& other);

  std::string ToString() const;

  size_t HashCode() const;
  bool Equals(const ConnectionId& other) const;

 private:
  Sockaddr remote_;
  UserCredentials user_credentials_;

  void DoCopyFrom(const ConnectionId& other);
  void operator=(const ConnectionId&);
};

class ConnectionIdHash {
 public:
  size_t operator()(const ConnectionId& conn_id) const;
};

class ConnectionIdEqual {
 public:
  bool operator()(const ConnectionId& id1, const ConnectionId& id2) const;
};

class OutboundCall {
 public:
  OutboundCall(const ConnectionId& conn_id,
               const RemoteMethod& remote_method,
               google::protobuf::Message* response_storage,
               RpcController* controller,
               ResponseCallback callback);
  ~OutboundCall();

  void SetRequestParam(const google::protobuf::Message& request);
  void set_call_id(int32_t call_id);

  Status SerializeTo(std::vector<Slice>* slices);

  void SetQueued();
  void SetSending();
  void SetSent();
  void SetFailed(const Status& status, ErrorStatusPB* err_pb = nullptr);

  void SetTimedOut();
  bool IsTimedOut() const;

  bool IsFinished() const;

  void SetResponse(gscoped_ptr<CallResponse> resp);

  const std::set<RpcFeatureFlag>& required_rpc_features() const;

  std::string ToString() const;

  void DumpPB(const DumpRunningRpcsRequestPB& req, RpcCallInProcessPB* resp);

  // Getters
  const ConnectionId& conn_id() const;
  const RemoteMethod& remote_method() const;
  const ResponseCallback& callback() const;
  RpcController* controller();
  const RpcController* controller() const;

  bool call_id_assigned() const;
  int32_t call_id() const;

 private:
  friend class RpcController;

  enum State {
    READY = 0,
    ON_OUTBOUND_QUEUE,
    SENDING,
    SENT,
    TIMED_OUT,
    FINISHED_ERROR,
    FINISHED_SUCCESS,
  };
  
  static std::string StateName(State state);

  void set_state(State new_state);
  State state() const;

  void set_state_unlocked(State new_state);

  Status status() const;

  MonoTime start_time_;
  const ErrorStatusPB* error_pb() const;

  mutable simple_spinlock lock_;
  State state_;
  Status status_;
  gscoped_ptr<ErrorStatusPB> error_pb_;

  void CallCallback();

  RequestHeader header_;
  RemoteMethod remote_method_;

  std::set<RpcFeatureFlag> required_rpc_features_;

  ConnectionId conn_id_;
  ResponseCallback callback_;
  RpcController* controller_;

  google::protobuf::Message* response_;

  faststring header_buf_;
  faststring request_buf_;

  gscoped_ptr<CallResponse> call_response_;

  DISALLOW_COPY_AND_ASSIGN(OutboundCall);
};

class CallResponse {
 public:
  CallResponse();

  Status ParseFrom(gscoped_ptr<InboundTransfer> transfer);
  bool is_success() const;
  int32_t call_id() const;
  const Slice& serialized_response() const;
  Status GetSidecar(int idx, Slice* sidecar) const;

 private:
  bool parsed_;
  ResponseHeader header_;
  Slice serialized_response_;
  Slice sidecar_slices_[OutboundTransfer::kMaxPayloadSlices];
  gscoped_ptr<InboundTransfer> transfer_;

  DISALLOW_COPY_AND_ASSIGN(CallResponse);
};

} // namespace rpc
} // namespace bb
