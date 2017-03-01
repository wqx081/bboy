#ifndef BBOY_BASE_NET_NET_UTIL_H_
#define BBOY_BASE_NET_NET_UTIL_H_
#include <string>
#include <vector>

#include "bboy/base/status.h"

namespace bb {

class Sockaddr;

class HostPort {
 public:
  HostPort();
  HostPort(std::string host, uint16_t port);
  explicit HostPort(const Sockaddr& addr);

  Status ParseString(const std::string& str, uint16_t default_port);
  Status ResolveAddresses(std::vector<Sockaddr>* addresses) const;

  std::string ToString() const;

  const std::string& host() const { return host_; }

  void set_host(const std::string& host) { host_ = host; }

  uint16_t port() const { return port_; }

  void set_port(uint16_t port) { port_ = port; }

  size_t HashCode() const;

  static Status ParseStrings(const std::string& comma_sep_addrs,
		             uint16_t default_port,
			     std::vector<HostPort>* res);
  static std::string ToCommaSeparatedString(const std::vector<HostPort>&
		                            host_ports);

 private:
  std::string host_;
  uint16_t port_;
};

bool operator==(const HostPort& hp1, const HostPort& hp2);

struct HostPortHasher {
  size_t operator()(const HostPort& hp) const {
    return hp.HashCode();
  }
};

struct HostPortEqualityPredicate {
  bool operator()(const HostPort& hp1, const HostPort& hp2) const {
    return hp1 == hp2;
  }
};

Status ParseAddressList(const std::string& addr_list,
		        uint16_t default_port,
			std::vector<Sockaddr>* addresses);
bool IsPrivilegedPort(uint16_t port);
Status GetHostname(std::string* hostname);
Status GetFQDN(std::string* fqdn);
Status SockaddrFromHostPort(const HostPort& host_port,
		            Sockaddr* addr);
Status HostPortFromSockaddrReplaceWildcard(const Sockaddr& addr,
		                           HostPort* hp);

} // namespace bb
#endif // BBOY_BASE_NET_NET_UTIL_H_
