#ifndef BBOY_BASE_NET_SOCKET_H_
#define BBOY_BASE_NET_SOCKET_H_
#include <sys/uio.h>
#include <string>

#include "bboy/gbase/macros.h"
#include "bboy/base/status.h"

namespace bb {

class MonoDelta;
class MonoTime;
class Sockaddr;

class Socket {
 public:
  static const int FLAG_NONBLOCKING = 0x1;

  Socket();
  explicit Socket(int fd);
  virtual ~Socket();

  virtual Status Close();
  Status Shutdown(bool shut_read, bool shut_write);
  void Reset(int fd);
  int Release();
  int GetFd() const;
  static bool IsTemporarySocketError(int err);
  Status Init(int flags); // Create a underlying socket
  Status SetNoDelay(bool enabled);
  Status SetTcpCork(bool enabled);
  Status SetNonBlocking(bool enabled);
  Status IsNonBlocking(bool* is_nonblock) const;

  Status SetSendTimeout(const MonoDelta& timeout);
  Status SetRecvTimeout(const MonoDelta& timeout);
 
  Status SetReuseAddr(bool flag);

  Status BindAndListen(const Sockaddr& sockaddr, int listen_queue_size);
  Status Listen(int listen_queue_size);

  Status GetSocketAddress(Sockaddr* cur_addr) const;
  Status GetPeerAddress(Sockaddr* cur_addr) const;

  bool IsLoopbackConnection() const;

  Status Bind(const Sockaddr& bin_addr);
  Status Accept(Socket* new_conn, Sockaddr* remote, int flags);
  Status Connect(const Sockaddr& remote);
  Status GetSockError() const;

  virtual Status Write(const uint8_t* buf, size_t amt, size_t* nwritten);
  virtual Status Writev(const struct ::iovec* iov, int iov_len, size_t* nwritten);
  Status BlockingWrite(const uint8_t* buf, size_t buf_len, size_t* nwritten, const MonoTime& deadline);


  virtual Status Read(uint8_t* buf, size_t amt, size_t* nread);
  Status BlockingRead(uint8_t* buf, size_t amt, size_t* nread, const MonoTime& dealline);

 private:
  Status SetTimeout(int opt, std::string optname, const MonoDelta& timeout);
  Status SetCloseOnExec();
  Status BindForOutgoingConnection();

  int fd_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

} // namespace bb
#endif // BBOY_BASE_NET_SOCKET_H_
