//
//
//
#ifndef BBOY_BASE_SLICE_H_
#define BBOY_BASE_SLICE_H_

#include <assert.h>
#include <map>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>

#include "bboy/gbase/strings/fastmem.h"
#include "bboy/gbase/strings/stringpiece.h"
#include "bboy/base/faststring.h"

namespace bb {

class Status;

class Slice {
 public:
  Slice() : data_(reinterpret_cast<const uint8_t*>("")),
	    size_(0) {}

  Slice(const uint8_t* d, size_t n)
    : data_(d),
      size_(n) {}

  Slice(const char* d, size_t n)
    : data_(reinterpret_cast<const uint8_t *>(d)),
      size_(n) {}

  Slice(const std::string& s)
    : data_(reinterpret_cast<const uint8_t *>(s.data())),
      size_(s.size()) {}

  Slice(const char* s)
    : data_(reinterpret_cast<const uint8_t *>(s)),
      size_(strlen(s)) {}

  Slice(const faststring& s)
    : data_(s.data()),
      size_(s.size()) {}

  Slice(const StringPiece& s)
    : data_(reinterpret_cast<const uint8_t *>(s.data())),
      size_(s.size()) {}

  const uint8_t* data() const { return data_; }

  uint8_t* mutable_data() { return const_cast<uint8_t *>(data_); }

  size_t size() const { return size_; }

  bool empty() const { return size_ == 0; }

  const uint8_t& operator[](size_t n) const {
    assert(n < size());
    assert(n >= 0);
    return data_[n];
  }

  void clear() {
    data_ = reinterpret_cast<const uint8_t *>("");
    size_ = 0;
  }

  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  void truncate(size_t n) {
    assert(n <= size());
    size_ = n;
  }

  //TODO(wqx):
  // Status check_size(size_t expected_size) const;
  
  std::string ToString() const;
  std::string ToDebugString(size_t max_len = 0) const;

  int compare(const Slice& b) const;

  bool starts_with(const Slice& x) const {
    return ((size_ >= x.size_) &&
            (MemEqual(data_, x.data_, x.size_)));
  }

  struct Comparator {
    bool operator()(const Slice& a, const Slice& b) const {
      return a.compare(b) < 0;
    }
  };

  void relocate(uint8_t* d) {
    if (data_ != d) {
      memcpy(d, data_, size_);
      data_ = d;
    }
  }

 private:
  friend bool operator==(const Slice& x, const Slice& y);

  static bool MemEqual(const void* a, const void* b, size_t n) {
    return strings::memeq(a, b, n);
  }

  static int MemCompare(const void* a, const void* b, size_t n) {
    return strings::fastmemcmp_inlined(a, b, n);
  }

  const uint8_t* data_;
  size_t size_;
};


inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (Slice::MemEqual(x.data(), y.data(), x.size())));
}

inline bool operator!=(const Slice& x, const Slice& y) {
  return !(x == y);
}

inline std::ostream& operator<<(std::ostream& o,
		                const Slice& s) {
  return o << s.ToDebugString(16);
}

inline int Slice::compare(const Slice& b) const {
  const int min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = MemCompare(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

// Example:
//
//  typedef SliceMap<int>::type MySliceMap;
//
//  MySliceMap my_map;
//  my_map.insert(MySliceMap::value_type(a, 1));
//  my_map.insert(MySliceMap::value_type(b, 2));
//  my_map.insert(MySliceMap::value_type(c, 3));
//
//  for (const MySliceMap::value_type& pair : my_map) {
//    ...
//  }
//
template <typename T>
struct SliceMap {
  typedef std::map<Slice, T, Slice::Comparator> type;
};

} // namespace bb
#endif // BBOY_BASE_SLICE_H_
