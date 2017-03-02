#include "bboy/base/net/socket.h"
#include "bboy/base/net/sockaddr.h"
#include "bboy/base/net/net_util.h"
#include "bboy/base/monotime.h"

#include <gtest/gtest.h>

namespace bb {

class EchoServer {
 public:
  EchoServer(const std::string& host, uint16_t port)
    : hostpost_(host, port) {}

  Status Start() {
    Status status;
    Sockaddr bind_addr_;

    DCHECK_OK(listen_socket_.Init(0));
    DCHECK_OK(bind_addr_.ParseString(hostpost_.host(), hostpost_.port()));
    DCHECK_OK(listen_socket_.BindAndListen(bind_addr_, 511));

    for (;;) {
      Socket new_sock;
      Sockaddr remote_addr;
      LOG(INFO) << "Listen at: " << hostpost_.ToString();
      RETURN_NOT_OK(listen_socket_.Accept(&new_sock, &remote_addr, 0)); // Blocking
      LOG(INFO) << "Accepted from: " << remote_addr.ToString();
      // Handle read
      uint8_t buf[1024];
      size_t nread;
      size_t nwritten;

      RETURN_NOT_OK(new_sock.Read(buf, 1024, &nread));
      LOG(INFO) << "recv: " << std::string(reinterpret_cast<const char*>(buf), nread);

      // Handle write
      RETURN_NOT_OK(new_sock.Write(buf, nread, &nwritten));
      new_sock.Close();
      break;
    }
    return Status::OK();  
  }

 private:
  HostPort hostpost_;
  Socket listen_socket_;
};

TEST(Socket, EchoServer) {
  EchoServer server("0.0.0.0", 9788);
  server.Start();
}

} // namespace bb
