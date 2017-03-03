#include "bboy/base/sync/once.h"
#include "bboy/base/malloc.h"

namespace bb {

size_t BBoyOnceDynamic::memory_footprint_excluding_this() const {
  return status_.memory_footprint_excluding_this();
}

size_t BBoyOnceDynamic::memory_footprint_including_this() const {
  return bboy_malloc_usable_size(this) + memory_footprint_excluding_this();
}

} // namespace bb
