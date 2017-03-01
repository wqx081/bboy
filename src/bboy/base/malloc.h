#ifndef BBOY_BASE_MALLOC_H
#define BBOY_BASE_MALLOC_H

#include <stdint.h>

namespace bb {

int64_t bboy_malloc_usable_size(const void* obj);

} // namespace bb
#endif // BBOY_BASE_MALLOC_H
