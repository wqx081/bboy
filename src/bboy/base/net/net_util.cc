#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <algorithm>
#include <boost/functional/hash.hpp>
#include <gflags/gflags.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/map-util.h"
#include "bboy/gbase/strings/join.h"
#include "bboy/gbase/strings/numbers.h"
#include "bboy/gbase/strings/split.h"
#include "bboy/gbase/strings/strip.h"
#include "bboy/gbase/strings/substitute.h"
#include "bboy/gbase/strings/util.h"
#include "bboy/base/errno.h"
#include "bboy/base/faststring.h"

#include "bboy/base/net/net_util.h"
#include "bboy/base/net/sockaddr.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif


DEFINE_bool(fail_dns_resolution, false, "Whether to fail all dns resolution, for tests.");

using std::unordered_set;
using std::vector;
using strings::Substitute;

namespace bb {

namespace {
struct AddrinfoDeleter {
  void operator()(struct addrinfo* info) {
    freeaddrinfo(info);
  }
};
} // namespace


HostPort::HostPort()
  : host_(""),
    port_(0) {
}

HostPort::HostPort(std::string host, uint16_t port)
  : host_(std::move(host)),
    port_(port) {
 }

HostPort::HostPort(const Sockaddr& addr)
  : host_(addr.host()),
    port_(addr.port()) {
}

bool operator==(const HostPort& hp1, const HostPort& hp2) {
  return hp1.port() == hp2.port() && hp1.host() == hp2.host();
}

size_t HostPort::HashCode() const {
  size_t seed = 0;
  boost::hash_combine(seed, host_);
  boost::hash_combine(seed, port_);
  return seed;
}

Status HostPort::ParseString(const std::string& str,
		             uint16_t default_port) {
  std::pair<std::string, std::string> p = strings::Split(str,
		       strings::delimiter::Limit(":", 1));
  StripWhiteSpace(&p.first);

  uint32_t port;
  if (p.second.empty() && strcount(str, ':') == 0) {
    port = default_port;
  } else if (!SimpleAtoi(p.second, &port) ||
             port > 65535) {
    return Status::InvalidArgument("Invalid port", str);
  }

  host_.swap(p.first);
  port_ = port;
  return Status::OK();
}

Status HostPort::ResolveAddresses(std::vector<Sockaddr>* addresses) const {

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  int rc;
  rc = getaddrinfo(host_.c_str(), nullptr, &hints, &res);
  if (rc != 0) {
    return Status::NetworkError(
      StringPrintf("Unable to resolve address '%s'", host_.c_str()),
	                                             gai_strerror(rc));
  }
  gscoped_ptr<addrinfo, AddrinfoDeleter> scoped_res(res);
  for (; res != nullptr; res = res->ai_next) {
    CHECK_EQ(res->ai_family, AF_INET);
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    addr->sin_port = htons(port_);
    Sockaddr sockaddr(*addr);
    if (addresses) {
      addresses->push_back(sockaddr);
    }
    VLOG(2) << "Resolved address " << sockaddr.ToString()
	    << " for host/port " << ToString();
  }

  if (PREDICT_FALSE(FLAGS_fail_dns_resolution)) {
    return Status::NetworkError("injected DNS resolution failure");
  }
  return Status::OK();
}

Status HostPort::ParseStrings(const std::string& comma_sep_addrs,
		              uint16_t default_port,
			      vector<HostPort>* res) {
  vector<std::string> addr_strings = strings::Split(comma_sep_addrs,
		                       ",", strings::SkipEmpty());

  for (const std::string& addr_string : addr_strings) {
    HostPort host_port;
    RETURN_NOT_OK(host_port.ParseString(addr_string, default_port));
    res->push_back(host_port);
  }
  return Status::OK();
}

std::string HostPort::ToString() const {
  return Substitute("$0:$1", host_, port_);
}

std::string HostPort::ToCommaSeparatedString(
		const vector<HostPort>& hostports) {
  vector<std::string> hostport_strs;
  for (const HostPort& hostport : hostports) {
    hostport_strs.push_back(hostport.ToString());
  }
  return JoinStrings(hostport_strs, ",");
}

bool IsPrivilegedPort(uint16_t port) {
  return port <= 1024 && port != 0;
}

Status ParseAddressList(const std::string& addr_list,
		        uint16_t default_port,
			std::vector<Sockaddr>* addresses) {
  vector<HostPort> host_ports;
  RETURN_NOT_OK(HostPort::ParseStrings(addr_list, default_port, &host_ports));

  if (host_ports.empty()) return Status::InvalidArgument("No address specified");

  unordered_set<Sockaddr> uniqued;
  for (const HostPort& host_port : host_ports) {
    vector<Sockaddr> this_addresses;
    RETURN_NOT_OK(host_port.ResolveAddresses(&this_addresses));

    for (const Sockaddr& addr : this_addresses) {
      if (InsertIfNotPresent(&uniqued, addr)) {
        addresses->push_back(addr);
      } else {
        LOG(INFO) << "Address " << addr.ToString() << " for " << host_port.ToString() << " duplicates an earlier resolved entry.";
      }
    }
  }
  return Status::OK();
}

Status GetHostname(std::string* hostname) {
  char name[HOST_NAME_MAX];
  int ret = gethostname(name, HOST_NAME_MAX);
  if (ret != 0) {
    return Status::NetworkError("Unable to determine local hostname",
		                 ErrnoToString(errno),
				 errno);
  }
  *hostname = name;
  return Status::OK();
}

Status GetFQDN(std::string* hostname) {
  RETURN_NOT_OK(GetHostname(hostname));

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  struct addrinfo* result;
  int rc = getaddrinfo(hostname->c_str(), nullptr, &hints, &result);
  if (rc != 0) {
    return Status::NetworkError("Unable to lookup FQDN", ErrnoToString(errno), errno);
  }

  *hostname = result->ai_canonname;
  freeaddrinfo(result);
  return Status::OK();
}

Status SockaddrFromHostPort(const HostPort& host_port, 
		            Sockaddr* addr) {
  vector<Sockaddr> addrs;
  RETURN_NOT_OK(host_port.ResolveAddresses(&addrs));
  if (addrs.empty()) {
    return Status::NetworkError("Unable to resolve address", 
		                host_port.ToString());
  }
  *addr = addrs[0];
  if (addrs.size() > 1) {
    VLOG(1) << "Hostname " << host_port.host() 
	    << " resolved to mo  re than one address. "
	    << "Using address: " << addr->ToString();
  }
  return Status::OK();
}
  
Status HostPortFromSockaddrReplaceWildcard(const Sockaddr& addr, 
		                           HostPort* hp) {
  string host;
  if (addr.IsWildcard()) {
    RETURN_NOT_OK(GetFQDN(&host));
  } else {
    host = addr.host();
  }
  hp->set_host(host);
  hp->set_port(addr.port());
  return Status::OK();
}

} // namespace bb
