#include "bboy/base/malloc.h"

#include <malloc.h>

namespace bb {

int64_t bboy_malloc_usable_size(const void* obj) {
  return malloc_usable_size(const_cast<void*>(obj));
}

} // namespace bb
