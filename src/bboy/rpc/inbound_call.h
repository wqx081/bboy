#pragma once

#include <glog/logging.h>
#include <string>
#include <vector>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/stl_util.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/ref_counted.h"

#include "bboy/rpc/remote_method.h"
#include "bboy/rpc/service_if.h"
#include "bboy/rpc/rpc_header.pb.h"
#include "bboy/rpc/transfer.h"

#include "bboy/base/faststring.h"
#include "bboy/base/monotime.h"
#include "bboy/base/slice.h"
#include "bboy/base/status.h"


namespace google {
namespace protobuf {
class Message;
} // namespace protobuf
} // namespace google

namespace bb {

class Histogram;
class Trace;

namespace rpc {

class Connection;
class DumpRunningRpcsRequestPB;
class RemoteUser;
class RpcCallInProgressPB;
struct RpcMethodInfo;
class RpcSidecar;

struct InboundCallTiming {
  MonoTime time_received;
  MonoTime time_handled;
  MonoTime time_completed;

  MonoDelta TotalDuration() const;
};

class InboundCall {
 public:
  explicit InboundCall(Connection* conn);
  ~InboundCall();

  Status ParseFrom(gscoped_ptr<InboundTransfer> transfer);
  const Slice& serialized_request() const;
  const RemoteMethod& remote_method() const;
  const int32_t call_id() const;

  void RespondSuccess(const google::protobuf::MessageLite& response);
  void ResponseFailure(ErrorStatusPB::RpcErrorCodePB error_code,
                       const Status& status);
  void RespondUnsupportedFeature(const std::vector<uint32_t>& unsupported_features);
  void RespondApplicationError(int error_ext_id, const std::string& message,
                               const google::protobuf::MessageLite& app_error_pb);
  static void ApplicationErrorToPB(int error_ext_id, const std::string& message,
                                   const google::protobuf::MessageLite& app_error_pb,
                                   ErrorStatusPB* err);

  void SerializeResponseTo(std::vector<Slice>* slices) const;
  Status AddRpcSidecar(gscoped_ptr<RpcSidecar> car, int* idx) const;
  std::string ToString() const;

  void DumpPB(const DumpRunningRpcsRequestPB& req, RpcCallInProgressPB* resp);
  const RemoteUser& remote_user() const;
  const Sockaddr& remote_address() const;
  const scoped_refptr<Connection>& connection() const;
  
  Trace* trace();

  const InboundCallTiming& timing() const;
  const RequestHeader& header() const;

  void set_method_info(scoped_refptr<RpcMethodInfo> info);
  RpcMethodInfo* method_info();
  void RecordCallReceived();
  void RecordHandlingStarted(scoped_refptr<Histogram> incoming_queue_time);
  bool ClientTimedOut() const;
  MonoTime GetClientDeadline() const;
  MonoTime GetTimeReceived() const;
  std::vector<uint32_t> GetRequiredFeatures() const;

 private:
  friend class RpczStore;

  void Respond(const google::protobuf::MessageLite& response,
               bool is_success);
  void SerializeResponseBuffer(const google::protobuf::MessageLite& response,
                               bool is_success);
  void RecordHandlingCompleted();
  scoped_refptr<Connection> conn_;
  RequestHeader header_;
  Slice serialized_request_;
  gscoped_ptr<InboundTransfer> transfer_;

  faststring response_hdr_buf_;
  faststring response_msg_buf_;

  std::vector<RpcSidecar*> sidecars_;
  ElementDeleter sidecars_deleter_;

  scoped_refptr<Trace> trace_;

  InboundCallTiming timing_;

  RemoteMethod remote_method_;

  scoped_refptr<RpcMethodInfo> method_info_;

  DISALLOW_COPY_AND_ASSIGN(InboundCall);
};

} // namespace rpc
} // namespace bb
