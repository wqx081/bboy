#include "bboy/rpc/acceptor_pool.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "bboy/gbase/ref_counted.h"
#include "bboy/gbase/strings/substitute.h"
#include "bboy/rpc/messenger.h"

DEFINE_int32(rpc_acceptor_listen_backlog, 511, /* from nginx */
  "Socket backlog parameter used when listening for RPC connections. "
  "This defines the maximum length to which the queue of pending "
  "TCP connections inbound to the RPC server may grow. If a connection "
  "request arrives when the queue is full, the client may receive "
  "an error. Higher values may help the server ride over bursts of "
  "new inbound connection requests.");

namespace bb {
namespace rpc {

AcceptorPool::AcceptorPool(Messenger* messenger,
                           Socket* socket,
                           Sockaddr bind_address)
    : messenger_(messenger),
      socket_(socket->Release()),
      bind_address_(std::move(bind_address)),
      closing_(false) {}

AcceptorPool::~AcceptorPool() {
  Shutdown();
}

void AcceptorPool::Shutdown() {
  if (Acquire_CompareAndSwap(&closing_, false, true) != false) {
    VLOG(2) << "Acceptor Pool on " << bind_address_.ToString() << " already shut down";
    return;
  }

  WARN_NOT_OK(socket_.Shutdown(true, true),
              strings::Substitute("Could not shutdown acceptor socket on $0",
                                  bind_address_.ToString()));

  for (const scoped_refptr<Thread>& thread : threads_) {
    CHECK_OK(ThreadJoiner(thread.get()).Join());
  }
  threads_.clear();
}

// 创建多个 Acceptor 线程监听
Status AcceptorPool::Start(int num_threads) {
  RETURN_NOT_OK(socket_.Listen(FLAGS_rpc_acceptor_listen_backlog));

  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<Thread> new_thread;
    Status s = Thread::Create("acceptor pool", "acceptor",
                              &AcceptorPool::RunThread,
                              this,
                              &new_thread);
    if (!s.ok()) {
      Shutdown();
      return s;
    }
    threads_.push_back(new_thread);
  }
  return Status::OK();
}

void AcceptorPool::RunThread() {
  // 循环调用非阻塞的 accept(), 在没有新连接的情况下会有耗 CPU 的可能
  while (true) {
    Socket new_sock;
    Sockaddr remote;
    VLOG(2) << "calling accept() on socket " << socket_.GetFd()
              << " listening on " << bind_address_.ToString();
    Status s = socket_.Accept(&new_sock, &remote, Socket::FLAG_NONBLOCKING);
    if (!s.ok()) {
      if (Release_Load(&closing_)) {
        break;
      }
      VLOG(2) << "AcceptorPool: accept failed: " << s.ToString();
      continue;
    }
    s = new_sock.SetNoDelay(true);
    if (!s.ok()) {
      LOG(WARNING) << "Acceptor with remote = " << remote.ToString()
                << " failed to set TCP_NODELAY on a newly accepted socket: "
                << s.ToString();
      continue;
    }
    Sockaddr r;
    new_sock.GetPeerAddress(&r);
    LOG(INFO) << "Accept A new socket: " << r.ToString();
    //TODO(wqx):
    //rpc_connections_accepted_->Increment();
    //messenger_->RegisterInboundSocket(&new_sock, remote);
    //
    break;
  }
  LOG(INFO) << "AcceptorPool shutting down. ";
}

Sockaddr AcceptorPool::bind_address() const { 
  return bind_address_; 
}

Status AcceptorPool::GetBoundAddress(Sockaddr* addr) const {
  return socket_.GetSocketAddress(addr);
}

} // namespace rpc
} // namespace bb
