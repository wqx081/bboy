#ifndef BBOY_UTIL_STATUS_CALLBACK_H
#define BBOY_UTIL_STATUS_CALLBACK_H

#include <string>

#include "bboy/gbase/callback_forward.h"

namespace bb {

class Status;

// A callback which takes a Status. This is typically used for functions which
// produce asynchronous results and may fail.
typedef Callback<void(const Status& status)> StatusCallback;

// To be used when a function signature requires a StatusCallback but none
// is needed.
extern void DoNothingStatusCB(const Status& status);

// A callback that crashes with a FATAL log message if the given Status is not OK.
extern void CrashIfNotOkStatusCB(const std::string& message, const Status& status);

// A closure (callback without arguments) that returns a Status indicating
// whether it was successful or not.
typedef Callback<Status(void)> StatusClosure;

// To be used when setting a StatusClosure is optional.
extern Status DoNothingStatusClosure();

} // namespace kudu

#endif
