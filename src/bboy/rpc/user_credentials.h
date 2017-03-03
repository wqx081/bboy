#pragma once

#include <stdint.h>
#include <string>

namespace bb {
namespace rpc {

class UserCredentials {
 public:
  bool has_real_user() const;
  void set_real_user(const std::string& real_user);
  const std::string& real_user() const { return real_user_; }

  std::string ToString() const;

  size_t HashCode() const;
  bool Equals(const UserCredentials& other) const;

 private:
  std::string real_user_;
};

} // namespace rpc
} // namespace bb
