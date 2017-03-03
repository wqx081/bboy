#include <gtest/gtest.h>

#include "bboy/base/net/socket.h"
#include "bboy/base/net/sockaddr.h"

#include "bboy/rpc/messenger.h"
#include "bboy/rpc/acceptor_pool.h"

#include "bboy/base/monotime.h"

namespace bb {

TEST(AcceptorPool, Basic) {
  Sockaddr bind_addr; //("0.0.0.0", 9876);  
  Socket listen_socket;
  listen_socket.Init(Socket::FLAG_NONBLOCKING);
  DCHECK_OK(bind_addr.ParseString("0.0.0.0", 9876));
  DCHECK_OK(listen_socket.Bind(bind_addr)); 

  rpc::AcceptorPool pool(nullptr, &listen_socket, bind_addr);
  DCHECK_OK(pool.Start(2));
  
  SleepFor(MonoDelta::FromSeconds(10));
  pool.Shutdown();
}

} // namespace bb
