// faststring 与 std::string类似
// 它对一些常用的操作做了一些优化, 比如, resize() 操作并不会初始数据为'\0'.
//

#ifndef BBOY_BASE_FASTSTRING_H_
#define BBOY_BASE_FASTSTRING_H_

#include <string>
#include <glog/logging.h>

#include "bboy/gbase/dynamic_annotations.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/strings/fastmem.h"

namespace bb {

class faststring {
 public:
  faststring()
    : data_(initial_data_),
      len_(0),
      capacity_(kInitialCapacity) {
  }

  explicit faststring(size_t capacity)
    : data_(initial_data_),
      len_(0),
      capacity_(kInitialCapacity) {
    if (capacity > capacity_) {
      data_ = new uint8_t[capacity];
      capacity_ = capacity;
    }
    ASAN_POISON_MEMORY_REGION(data_, capacity_);
  }

  ~faststring() {
    ASAN_UNPOISON_MEMORY_REGION(initial_data_, arraysize(initial_data_));
    if (data_ != initial_data_) {
      delete[] data_;
    }
  }

  void clear() {
    resize(0);
    ASAN_POISON_MEMORY_REGION(data_, capacity_);
  }

  void resize(size_t new_size) {
    if (new_size > capacity_) {
      reserve(new_size);
    } 
    len_ = new_size;
    ASAN_POISON_MEMORY_REGION(data_ + len_, capacity_ - len_);
    ASAN_UNPOISON_MEMORY_REGION(data_, len_);
  }

  uint8_t* release() WARN_UNUSED_RESULT {
    uint8_t* ret = data_;
    if (ret == initial_data_) {
      ret = new uint8_t[len_];
      memcpy(ret, data_, len_);
    }
    len_ = 0;
    capacity_ = kInitialCapacity;
    data_ = initial_data_;
    ASAN_POISON_MEMORY_REGION(data_, capacity_);
    return ret;
  }

  void reserve(size_t new_capacity) {
    if (PREDICT_TRUE(new_capacity <= capacity_)) return;
    GrowArray(new_capacity); // GrowByAtLeast() ???
  }

  void append(const void* src_v, size_t count) {
    const uint8_t* src = reinterpret_cast<const uint8_t*>(src_v);
    EnsureRoomForAppend(count);
    ASAN_UNPOISON_MEMORY_REGION(data_ + len_, count);

    if (count <= 4) {
      uint8_t* p = &data_[len_];
      for (int i = 0; i < count; i++) {
        *p++ = *src++;
      }
    } else {
      strings::memcpy_inlined(&data_[len_], src, count);
    }
    len_ += count;
  }

  void append(const std::string& src) {
    append(src.data(), src.size());
  }

  void push_back(const char byte) {
    EnsureRoomForAppend(1);
    ASAN_UNPOISON_MEMORY_REGION(data_ + len_, 1);
    data_[len_++] = byte;
  }

  size_t length() const { return len_; }
  size_t size() const { return len_; }
  size_t capacity() const { return capacity_; }

  const uint8_t* data() const {
    return &data_[0];
  }

  uint8_t* data() {
    return &data_[0];
  }

  const uint8_t& at(size_t i) const {
    DCHECK(i >= 0 && i < length());
    return data_[i];
  }

  const uint8_t& operator[](size_t i) const {
    DCHECK(i >= 0 && i < length());
    return data_[i];
  }

  uint8_t& operator[](size_t i) {
    DCHECK(i >= 0 && i < length());
    return data_[i];
  }

  void assign_copy(const uint8_t* src, size_t len) {
    len_ = 0;
    resize(len);
    memcpy(data(), src, len);
  }

  void assign_copy(const std::string& src) {
    assign_copy(reinterpret_cast<const uint8_t*>(src.c_str()),
                len_);
  }
  
  std::string ToString() const {
    return std::string(reinterpret_cast<const char*>(data_), len_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(faststring);

  void EnsureRoomForAppend(size_t count) {
    if (PREDICT_TRUE(len_ + count <= capacity_)) {
      return;       
    }

    GrowByAtLeast(count);
  }

  void GrowByAtLeast(size_t count);
  void GrowArray(size_t new_capacity);

  enum {
    kInitialCapacity = 32
  };

  uint8_t* data_;
  uint8_t initial_data_[kInitialCapacity];
  size_t len_;
  size_t capacity_;
};

} // namespace bb
#endif // BBOY_BASE_FASTSTRING_H_
