#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bboy/gbase/ref_counted.h"
#include "bboy/gbase/stl_util.h"

#include "bboy/base/sync/countdown_latch.h"
#include "bboy/base/sync/locks.h"
#include "bboy/base/thread/thread.h"
#include "bboy/base/malloc.h"
#include "bboy/base/monotime.h"
#include "bboy/base/mem_tracker.h"

#include "bboy/rpc/request_tracker.h"
#include "bboy/rpc/rpc_header.pb.h"

namespace google {
namespace protobuf {
class Message;
} // protobuf
} // google


namespace bb {
namespace rpc {

class RpcContext;

class ResultTracker : public RefCountedThreadSafe<ResultTracker> {
 public:
  typedef rpc::RequestTracker::SequenceNumber SequenceNumber;
  static const int NO_HANDLER = -1;

  enum RpcState {
    NEW,
    COMPLETED,
    IN_PROGRESS,
    STALE,
  };

  explicit ResultTracker(std::shared_ptr<MemTracker> mem_tracker);
  ~ResultTracker();

  RpcState TrackRpc(const RequestIdPB& request_id,
                    google::protobuf::Message* response,
                    RpcContext* context);
  RpcState TrackRpcOrChangeDriver(const RequestIdPB& request_id);
  bool IsCurrentDriver(const RequestIdPB& request_id);
  void RecordCompletionAndRespond(const RequestIdPB& request_id,
                                  const google::protobuf::Message* response);
  void FailAndRespond(const RequestIdPB& request_id,
                      google::protobuf::Message* response);
  void FailAndRespond(const RequestIdPB& request_id,
                      ErrorStatusPB_RpcErrorCodePB err,
                      const Status& status);
  void FailAndRespond(const RequestIdPB& request_id,
                      int error_ext_id,
                      const std::string& message,
                      const google::protobuf::Message& app_error_pb);
  void StartGCThread();
  void GCResults();

  std::string ToString();

 private:
  struct OnGoingRpcInfo {
    google::protobuf::Message* response;
    RpcContext* context;
    int64_t handler_attempt_no;

    std::string ToString() const;
  };

  struct CompletionRecord {
    CompletionRecord(RpcState state, int64_t driver_attempt_no);
    
    RpcState state;
    int64_t drvier_attempt_no;
    MonoTime last_updated;
    std::unique_ptr<google::protobuf::Message> response;
    std::vector<OnGoingRpcInfo> ongoing_rpcs;

    std::string ToString() const;

    int64_t memory_footprint() const;
  };

  struct ClientState {
    typedef MemTrackerAllocator<
      std::pair<const SequenceNumber,
                std::unique_ptr<CompletionRecord>>> CompletionRecordMapAllocator;
    typedef std::map<SequenceNumber,
                     std::unique_ptr<CompletionRecord>,
                     std::less<SequenceNumber>,
                     CompletionRecordMapAllocator> CompletionRecordMap;

    explicit ClientState(std::shared_ptr<MemTracker> mem_tracker);

    MonoTime last_heard_from;
    SequenceNumber stale_before_seq_no;
    CompletionRecordMap completion_records;

    template<typename MustGcRecordFunc>
    void GCCompletionRecords(const std::shared_ptr<MemTracker>& mem_tracker,
                             MustGcRecordFunc func);

    std::string ToString() const;

    int64_t memory_footprint() const;
  };

  RpcState TrackRpcUnlocked(const RequestIdPB& request_id,
                            google::protobuf::Message* response,
                            RpcContext* context);

  typedef std::function<void(const OnGoingRpcInfo&)> HandleOngoingRpcFunc;

  void FailAndRespondInternal(const rpc::RequestIdPB& request_id,
                              HandleOngoingRpcFunc func);

  CompletionRecord* FindCompletionRecordOrNullUnlocked(const RequestIdPB& );
  CompletionRecord* FindCompletionRecordOrDieUnlocked(const RequestIdPB& );
  std::pair<ClientState*, CompletionRecord*> FindClientStateAndCompletionRecordOrNullUnlocked(
    const RequestIdPB& request_id);

  bool MustHandleRpc(int64_t handler_attempt_no,
                     CompletionRecord* completion_record,
                     const OnGoingRpcInfo& ongoing_rpc);

  void LogAndTraceAndRespondSuccess(RpcContext* context, const google::protobuf::Message& msg);
  void LogAndTraceFailure(RpcContext* context, const google::protobuf::Message& msg);
  void LogAndTraceFailure(RpcContext* context, ErrorStatusPB_RpcErrorCodePB err,
                          const Status& status);

  std::string ToStringUnlocked() const;

  void RunGCThread();

  std::shared_ptr<MemTracker> mem_tracker_;

  simple_spinlock lock_;

  typedef MemTrackerAllocator<std::pair<const std::string,
                                        std::unique_ptr<ClientState>>> ClientStateMapAllocator;

  typedef std::map<std::string,
                   std::unique_ptr<ClientState>,
                   std::less<std::string>,
                   ClientStateMapAllocator> ClientStateMap;

  ClientStateMap clients_;

  scoped_refptr<Thread> gc_thread_;
  CountDownLatch gc_thread_stop_latch_;

  DISALLOW_COPY_AND_ASSIGN(ResultTracker);
};

} // namespace rpc
} // namespace bb
