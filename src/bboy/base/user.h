#ifndef KUDU_UTIL_USER_H
#define KUDU_UTIL_USER_H

#include <string>

#include "bboy/base/status.h"

namespace bb {

// Get current logged-in user with getpwuid_r().
// user name is written to user_name.
Status GetLoggedInUser(std::string* user_name);

} // namespace bb

#endif // KUDU_UTIL_USER_H
