#pragma once

#include <set>
#include <string>

#include "bboy/gbase/ref_counted.h"

#include "bboy/base/sync/locks.h"
#include "bboy/base/status.h"

namespace bb {
namespace rpc {

class RequestTracker : public RefCountedThreadSafe<RequestTracker> {
 public:
  typedef int64_t SequenceNumber;
  static const RequestTracker::SequenceNumber NO_SEQ_NO;
  
  explicit RequestTracker(const std::string& client_id);

  Status NewSeqNo(SequenceNumber* seq_no);
  SequenceNumber FirstIncomplete();
  void RpcCompleted(const SequenceNumber& seq_no);
  const std::string& client_id();

 private:
  const std::string client_id_;
  simple_spinlock lock_;
  SequenceNumber next_;
  std::set<SequenceNumber> incomplete_rpcs_;
};

} // namespace rpc
} // namespace bb
