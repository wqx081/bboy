#pragma once

#include <string>

namespace bb {
namespace rpc {

class RemoteMethodPB;

class RemoteMethod {
 public:
  RemoteMethod() {}
  RemoteMethod(std::string service_name, std::string method_name);
  std::string service_name() const { return service_name_; }
  std::string method_name() const { return method_name_; }

  void FromPB(const RemoteMethodPB& pb);
  void ToPB(RemoteMethodPB* pb) const;

  std::string ToString() const;

 private:
  std::string service_name_;
  std::string method_name_;
};

} // namespace rpc
} // namespace bb
