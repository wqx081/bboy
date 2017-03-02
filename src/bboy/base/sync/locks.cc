#include "bboy/base/sync/locks.h"
#include "bboy/base/malloc.h"


namespace bb {

size_t percpu_rwlock::memory_footprint_excluding_this() const {
  return n_cpus_ * sizeof(padded_lock);
}

size_t percpu_rwlock::memory_footprint_including_this() const {
  return bboy_malloc_usable_size(this) + memory_footprint_excluding_this();
}

} // namespace kudu
