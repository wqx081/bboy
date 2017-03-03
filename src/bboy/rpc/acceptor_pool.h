#ifndef BBOY_RPC_ACCEPTOR_POOL_H_
#define BBOY_RPC_ACCEPTOR_POOL_H_

#include <vector>

#include "bboy/gbase/atomicops.h"
#include "bboy/base/thread/thread.h"
#include "bboy/base/net/socket.h"
#include "bboy/base/net/sockaddr.h"
#include "bboy/base/status.h"

namespace bb {

class Counter;
class Socket;

namespace rpc {

class Messenger;

class AcceptorPool {
 public:
  AcceptorPool(Messenger* messenger, Socket* listen_socket, Sockaddr bind_address);
  ~AcceptorPool();

  // Start listening and accepting connections.
  Status Start(int num_threads);
  void Shutdown();

  Sockaddr bind_address() const;
  Status GetBoundAddress(Sockaddr* addr) const;

 private:
  void RunThread();

  Messenger* messenger_;
  Socket socket_;
  Sockaddr bind_address_;
  std::vector<scoped_refptr<Thread>> threads_;

// TODO(wqx):
//  scoped_refptr<Counter> rpc_connections_accepted_;

  Atomic32 closing_;

  DISALLOW_COPY_AND_ASSIGN(AcceptorPool);
};
} // namespace rpc
} // namespace bb
#endif // BBOY_RPC_ACCEPTOR_POOL_H_
