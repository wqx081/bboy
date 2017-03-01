#ifndef BBOY_BASE_ERRNO_H_
#define BBOY_BASE_ERRNO_H_

#include <string>

namespace bb {

void ErrnoToCString(int err, char* buf, size_t buf_len);

inline static std::string ErrnoToString(int err) {
  char buf[512];
  ErrnoToCString(err, buf, sizeof(buf));
  return std::string(buf);
}

} // namespace bb
#endif // BBOY_BASE_ERRNO_H_
