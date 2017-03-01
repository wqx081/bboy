#include "bboy/base/errno.h"

#include <errno.h>
#include <string.h>

#include "bboy/gbase/dynamic_annotations.h"
#include <glog/logging.h>

namespace bb {

void ErrnoToCString(int err, char* buf, size_t buf_len) {
  CHECK_GT(buf_len, 0);

  ANNOTATE_IGNORE_WRITES_BEGIN();
  char* ret = strerror_r(err, buf, buf_len);
  ANNOTATE_IGNORE_WRITES_END();
  if (ret != buf) {
    strncpy(buf, ret, buf_len);
    buf[buf_len - 1] = '\0';
  }
}

} // namespace bb
