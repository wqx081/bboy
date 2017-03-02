#include "bboy/base/path_util.h"

// Use the POSIX version of dirname(3).
#include <libgen.h>

#include <glog/logging.h>
#include <string>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/strings/split.h"

#if defined(__APPLE__)
#include <mutex>
#endif // defined(__APPLE__)

using std::string;
using std::vector;
using strings::SkipEmpty;
using strings::Split;

namespace bb {

const char kTmpInfix[] = ".kudutmp";
const char kOldTmpInfix[] = ".tmp";

std::string JoinPathSegments(const std::string &a,
                             const std::string &b) {
  CHECK(!a.empty()) << "empty first component: " << a;
  CHECK(!b.empty() && b[0] != '/')
    << "second path component must be non-empty and relative: "
    << b;
  if (a[a.size() - 1] == '/') {
    return a + b;
  } else {
    return a + "/" + b;
  }
}

vector<string> SplitPath(const string& path) {
  if (path.empty()) return {};
  vector<string> segments;
  if (path[0] == '/') segments.push_back("/");
  vector<StringPiece> pieces = Split(path, "/", SkipEmpty());
  for (const StringPiece& piece : pieces) {
    segments.emplace_back(piece.data(), piece.size());
  }
  return segments;
}

string DirName(const string& path) {
  gscoped_ptr<char[], FreeDeleter> path_copy(strdup(path.c_str()));
#if defined(__APPLE__)
  static std::mutex lock;
  std::lock_guard<std::mutex> l(lock);
#endif // defined(__APPLE__)
  return ::dirname(path_copy.get());
}

string BaseName(const string& path) {
  gscoped_ptr<char[], FreeDeleter> path_copy(strdup(path.c_str()));
  return basename(path_copy.get());
}

} // namespace bb
