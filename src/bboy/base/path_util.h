//
// Utility methods for dealing with file paths.
#ifndef KUDU_UTIL_PATH_UTIL_H
#define KUDU_UTIL_PATH_UTIL_H

#include <string>
#include <vector>

namespace bb {

// Common tmp infix
extern const char kTmpInfix[];
// Infix from versions of Kudu prior to 1.2.
extern const char kOldTmpInfix[];

// Join two path segments with the appropriate path separator,
// if necessary.
std::string JoinPathSegments(const std::string &a,
                             const std::string &b);

// Split a path into segments with the appropriate path separator.
std::vector<std::string> SplitPath(const std::string& path);

// Return the enclosing directory of path.
// This is like dirname(3) but for C++ strings.
std::string DirName(const std::string& path);

// Return the terminal component of a path.
// This is like basename(3) but for C++ strings.
std::string BaseName(const std::string& path);

} // namespace bb
#endif /* KUDU_UTIL_PATH_UTIL_H */
