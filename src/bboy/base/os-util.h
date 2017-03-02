#ifndef BBOY_BASE_OS_UTIL_H_
#define BBOY_BASE_OS_UTIL_H_

#include <string>
#include "bboy/base/status.h"

namespace bb {

struct ThreadStats {
  int64_t user_ns;
  int64_t kernel_ns;
  int64_t iowait_ns;

  ThreadStats() : user_ns(0), kernel_ns(0), iowait_ns(0) {}
};

Status ParseStat(const std::string& buf, std::string* name, ThreadStats* stats);
Status GetThreadStats(int64_t tid, ThreadStats* stats);
void DisableCoreDumps();


} // namespace bb
#endif // BBOY_BASE_OS_UTIL_H_
