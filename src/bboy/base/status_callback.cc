#include "bboy/base/status_callback.h"
#include "bboy/base/status.h"

using std::string;

namespace bb {

void DoNothingStatusCB(const Status& status) {}

void CrashIfNotOkStatusCB(const string& message, const Status& status) {
  if (PREDICT_FALSE(!status.ok())) {
    LOG(FATAL) << message << ": " << status.ToString();
  }
}

Status DoNothingStatusClosure() { return Status::OK(); }

} // end namespace bb
