#pragma once

#include <boost/intrusive/list.hpp>
#include <gflags/gflags.h>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include "bboy/rpc/constants.h"
#include "bboy/base/net/sockaddr.h"
#include "bboy/base/status.h"

DECLARE_int32(rpc_max_message_size);

namespace google {
namespace protobuf {
class Message;
} // namespace protobuf
} // namespace google

namespace bb {

class Socket;

namespace rpc {

class Messenger;
class TransferCallbacks;

class InboundTransfer {
 public:
  InboundTransfer();
  Status ReceiveBuffer(Socket& socket);
  bool TransferStarted() const;
  bool TransferFinished() const;

  Slice data() const;

  std::string StatusAsString() const;

 private:
  Status ProcessInboundHeader();

  faststring buf_;

  int32_t total_length_;
  int32_t cur_offset_;

  DISALLOW_COPY_AND_ASSIGN(InboundTransfer);
};

class OutboundTransfer : public boost::intrusive::list_base_hook<> {
 public:
  enum { kMaxPayloadSlices = 10 };

  static OutboundTransfer* CreateForCallRequest(int32_t call_id,
                                                const std::vector<Slice>& payload,
                                                TransferCallbacks* callbacks);
  static OutboundTransfer* CreateForCallResponse(const std::vector<Slice>& payload,
                                                 TransferCallbacks* callbacks);

  ~OutboundTransfer();

  void Abort(const Status& status);
  Status SendBuffer(Socket& socket);
  bool TransferStarted() const;
  bool TransferFinished() const;

  int32_t TotalLength() const;
  std::string HexDump() const;

  bool is_for_outbound_call() const;
  int32_t call_id() const;

 private:
  OutboundTransfer(int32_t call_id,
                   const std::vector<Slice>& payload,
                   TransferCallbacks* callbacks);

  Slice payload_slices_[kMaxPayloadSlices];
  size_t n_payload_slices_;

  int32_t cur_slice_idx_;
  int32_t cur_offset_in_slice_;

  TransferCallbacks* callbacks_;

  int32_t call_id_;

  bool aborted_;

  DISALLOW_COPY_AND_ASSIGN(OutboundTransfer);
};

class TransferCallbacks {
 public:
  virtual ~TransferCallbacks();

  virtual void NotifyTransferFinished() = 0;
  virtual void NotifyTransferAborted(const Status& status) = 0;
};

} // namespace rpc
} // namespace bb
