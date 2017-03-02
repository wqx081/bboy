#include "bboy/base/sync/atomic.h"

#include <stdint.h>
#include <glog/logging.h>

namespace bb {

template<typename T>
AtomicInt<T>::AtomicInt(T initial_value) {
  Store(initial_value, kMemOrderNoBarrier);
}

template<typename T>
void AtomicInt<T>::FatalMemOrderNotSupported(const char* caller,
                                             const char* requested,
                                             const char* supported) {
  LOG(FATAL) << caller << " does not support " << requested << ": only "
             << supported << " are supported.";
}

template
class AtomicInt<int32_t>;

template
class AtomicInt<int64_t>;

template
class AtomicInt<uint32_t>;

template
class AtomicInt<uint64_t>;

AtomicBool::AtomicBool(bool value)
    : underlying_(value) {
}

} // namespace bb
