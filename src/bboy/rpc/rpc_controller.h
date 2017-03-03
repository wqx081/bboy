#pragma once

#include <functional>
#include <glog/logging.h>
#include <memory>
#include <unordered_set>

#include "bboy/gbase/macros.h"
#include "bboy/base/sync/locks.h"
#include "bboy/base/monotime.h"
#include "bboy/base/status.h"

namespace bb {
namespace rpc {

class ErrorStatusPB;
class OutboundCall;
class RequestIdPB;

class RpcController {
 public:
  RpcController();
  ~RpcController();

  void Swap(RpcController* other);
  void Reset();
  bool finished() const;
  Status status() const;
  const ErrorStatusPB* error_response() const;

  void set_timeout(const MonoDelta& timeout);
  void set_deadline(const MonoTime& deadline);

  void SetRequestIdPB(std::unique_ptr<RequestIdPB> request_id);

  bool has_request_id() const;

  const RequestIdPB& request_id() const;

  void RequireServerFeature(uint32_t feature);

  const std::unordered_set<uint32_t>& required_server_features() const;

  MonoDelta timeout() const;

  Status GetSidecar(int idx, Slice* sidecar) const;

 private:
  friend class OutboundCall;
  friend class Proxy;

  MonoDelta timeout_;
  std::unordered_set<uint32_t> required_server_features_;

  mutable simple_spinlock lock_;

  std::unique_ptr<RequestIdPB> request_id_;

  std::shared_ptr<OutboundCall> call_;

  DISALLOW_COPY_AND_ASSIGN(RpcController);
};

} // namespace rpc
} // namespace bb
