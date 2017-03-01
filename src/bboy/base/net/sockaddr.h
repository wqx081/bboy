#ifndef BBOY_BASE_NET_SOCKADDR_H_
#define BBOY_BASE_NET_SOCKADDR_H_

#include <netinet/in.h>
#include <iosfwd>
#include <string>

#include "bboy/base/status.h"

namespace bb {

class Sockaddr {
 public:
  Sockaddr();
  explicit Sockaddr(const struct sockaddr_in& addr);

  Status ParseString(const std::string& s, uint16_t default_port);

  Sockaddr& operator=(const struct sockaddr_in& addr);
  bool operator==(const Sockaddr& other) const;
  bool operator<(const Sockaddr& other) const;

  uint32_t HashCode() const;
  std::string host() const;

  void set_port(int port);
  int port() const;
  const struct sockaddr_in& addr() const;
  std::string ToString() const;

  bool IsWildcard() const;
  bool IsAnyLocalAddress() const;

  Status LookupHostname(std::string* hostname) const;

 private:
  struct sockaddr_in addr_;
};

} // namespace bb

namespace std {

template <>
struct hash<bb::Sockaddr> {
  int operator()(const bb::Sockaddr& addr) const {
    return addr.HashCode();
  }
};
} // namespace std
#endif // BBOY_BASE_NET_SOCKADDR_H_
