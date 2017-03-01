#include "bboy/base/net/sockaddr.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "bboy/gbase/endian.h"
#include "bboy/gbase/hash/builtin_type_hash.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/stringprintf.h"
#include "bboy/gbase/strings/substitute.h"

#include "bboy/base/net/net_util.h"

namespace bb {

using strings::Substitute;

Sockaddr::Sockaddr() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = INADDR_ANY;
}

Sockaddr::Sockaddr(const struct sockaddr_in& addr) {
  memcpy(&addr_, &addr, sizeof(struct sockaddr_in));
}

Status Sockaddr::ParseString(const std::string& s,
		             uint16_t default_port) {
  HostPort hp;
  RETURN_NOT_OK(hp.ParseString(s, default_port));

  if (inet_pton(AF_INET, hp.host().c_str(), &addr_.sin_addr) != 1) {
    return Status::InvalidArgument("Invalid IP address", hp.host());   
  }
  set_port(hp.port());
  return Status::OK();
}

Sockaddr& Sockaddr::operator=(const struct sockaddr_in &addr) {
  memcpy(&addr_, &addr, sizeof(struct sockaddr_in));
  return *this;
}
  
bool Sockaddr::operator==(const Sockaddr& other) const {
  return memcmp(&other.addr_, &addr_, sizeof(addr_)) == 0;
}
  
bool Sockaddr::operator<(const Sockaddr &rhs) const {
  return addr_.sin_addr.s_addr < rhs.addr_.sin_addr.s_addr;
}
  
uint32_t Sockaddr::HashCode() const {
  uint32_t hash = Hash32NumWithSeed(addr_.sin_addr.s_addr, 0);
  hash = Hash32NumWithSeed(addr_.sin_port, hash);
  return hash;
}
  
void Sockaddr::set_port(int port) {
  addr_.sin_port = htons(port);
}
  
int Sockaddr::port() const {
  return ntohs(addr_.sin_port);
}
  
std::string Sockaddr::host() const {
  char str[INET_ADDRSTRLEN];
  ::inet_ntop(AF_INET, &addr_.sin_addr, str, INET_ADDRSTRLEN);
  return str;
}
  
const struct sockaddr_in& Sockaddr::addr() const {
  return addr_;
}

std::string Sockaddr::ToString() const {
  char str[INET_ADDRSTRLEN];
  ::inet_ntop(AF_INET, &addr_.sin_addr, str, INET_ADDRSTRLEN);
  return StringPrintf("%s:%d", str, port());
}
  
bool Sockaddr::IsWildcard() const {
  return addr_.sin_addr.s_addr == 0;
}
  
bool Sockaddr::IsAnyLocalAddress() const {
  return (NetworkByteOrder::FromHost32(addr_.sin_addr.s_addr) >> 24) 
	  == 127;
}

Status Sockaddr::LookupHostname(string* hostname) const {
  char host[NI_MAXHOST];
  int flags = 0;
		  
  int rc;
  rc = getnameinfo((struct sockaddr *) &addr_, sizeof(sockaddr_in),
	          host, NI_MAXHOST, nullptr, 0, flags);

  if (PREDICT_FALSE(rc != 0)) {
    if (rc == EAI_SYSTEM) {
      int errno_saved = errno;
      return Status::NetworkError(Substitute("getnameinfo: $0", 
			      gai_strerror(rc)),
			      strerror(errno_saved), errno_saved);
    }
    return Status::NetworkError("getnameinfo", gai_strerror(rc), rc);
  }
  *hostname = host;
  return Status::OK();
}

} // namespace bb
