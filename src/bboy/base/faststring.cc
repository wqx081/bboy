#include "bboy/base/faststring.h"

#include <glog/logging.h>
#include "bboy/gbase/gscoped_ptr.h"

namespace bb {


void faststring::GrowByAtLeast(size_t count) {
  size_t to_reserve = len_ + count;
  if (len_ + count < len_ * 3 / 2) {
    to_reserve = len_ * 3 / 2;
  }
  GrowArray(to_reserve);
}

void faststring::GrowArray(size_t new_capacity) {
  DCHECK_GE(new_capacity, capacity_);
  gscoped_array<uint8_t> new_data(new uint8_t[new_capacity]);

  if (len_> 0) {
    memcpy(&new_data[0], &data_[0], len_);
  }
  capacity_ = new_capacity;
  if (data_ != initial_data_) {
    delete[] data_;
  } else {
    ASAN_POISON_MEMORY_REGION(initial_data_, arraysize(initial_data_));
  }

  data_ = new_data.release();
  ASAN_POISON_MEMORY_REGION(data_ + len_, capacity_ - len_);
}

} // namespace bb
