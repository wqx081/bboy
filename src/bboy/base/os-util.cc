#include "bboy/base/os-util.h"

#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <vector>
#include <unistd.h>

#include "bboy/gbase/strings/numbers.h"
#include "bboy/gbase/strings/split.h"
#include "bboy/gbase/strings/substitute.h"
#include "bboy/base/errno.h"

namespace bb {

#ifndef _SC_CLK_TCK
#define _SC_CLK_TCK 2
#endif

static const int64_t TICKS_PER_SEC = sysconf(_SC_CLK_TCK);

static const int64_t kUserTicks = 13 - 2;
static const int64_t kKernelTicks = 14 - 2;
static const int64_t kIoWait = 41 -2;

static const int64_t kMaxOffset = kIoWait;

Status ParseStat(const std::string& buf, std::string* name, ThreadStats* stats) {
  DCHECK(stats != nullptr);

  size_t open_paren = buf.find('(');
  size_t close_paren = buf.find(')');
  if (open_paren  == std::string::npos ||
      close_paren == std::string::npos ||
      open_paren >= close_paren        ||
      close_paren + 2 == buf.size()) {
    return Status::IOError("Unrecognised /proc format");
  }
  std::string extracted_name = buf.substr(open_paren + 1, close_paren - (open_paren + 1));
  std::string rest = buf.substr(close_paren + 2);
  std::vector<std::string> splits = strings::Split(rest, " ", strings::SkipEmpty());
  if (splits.size() < kMaxOffset) {
    return Status::IOError("Unrecognised /proc format");
  }

  int64 tmp;
  if (safe_strto64(splits[kUserTicks], &tmp)) {
    stats->user_ns = tmp * (1e9 / TICKS_PER_SEC);
  }
  if (safe_strto64(splits[kKernelTicks], &tmp)) {
    stats->kernel_ns = tmp * (1e9 / TICKS_PER_SEC);
  }
  if (safe_strto64(splits[kIoWait], &tmp)) {
    stats->iowait_ns = tmp * (1e9 / TICKS_PER_SEC);
  }
  if (name != nullptr) {
    *name = extracted_name;
  }
  return Status::OK();
}

Status GetThreadStats(int64_t tid, ThreadStats* stats) {
  DCHECK(stats != nullptr);
  if (TICKS_PER_SEC <= 0) {
    return Status::NotSupported("ThreadStats not supported");
  }

  std::ostringstream proc_path;
  proc_path << "/proc/self/task/" << tid << "/stat";
  std::ifstream proc_file(proc_path.str().c_str());
  if (!proc_file.is_open()) {
    return Status::IOError("Could not open ifstream");
  }

  std::string buffer((std::istreambuf_iterator<char>(proc_file)),
                      std::istreambuf_iterator<char>());

  return ParseStat(buffer, nullptr, stats); // don't want the name
}

void DisableCoreDumps() {
  struct rlimit lim;
  PCHECK(getrlimit(RLIMIT_CORE, &lim) == 0);
  lim.rlim_cur = 0;
  PCHECK(setrlimit(RLIMIT_CORE, &lim) == 0);

  int f = open("/proc/self/coredump_filter", O_WRONLY);
  if (f >= 0) {
    write(f, "00000000", 8);
    close(f);
  } 
} 


} // namespace bb
