#ifndef BBOY_UTIL_PROMISE_H
#define BBOY_UTIL_PROMISE_H

#include "bboy/gbase/macros.h"
#include "bboy/base/sync/countdown_latch.h"

namespace bb {

// A promise boxes a value which is to be provided at some time in the future.
// A single producer calls Set(...), and any number of consumers can call Get()
// to retrieve the produced value.
//
// In Guava terms, this is a SettableFuture<T>.
template<typename T>
class Promise {
 public:
  Promise() : latch_(1) {}
  ~Promise() {}

  // Reset the promise to be used again.
  // For this to be safe, there must be some kind of external synchronization
  // ensuring that no threads are still accessing the value from the previous
  // incarnation of the promise.
  void Reset() {
    latch_.Reset(1);
    val_ = T();
  }

  // Block until a value is available, and return a reference to it.
  const T& Get() const {
    latch_.Wait();
    return val_;
  }

  // Wait for the promised value to become available with the given timeout.
  //
  // Returns NULL if the timeout elapses before a value is available.
  // Otherwise returns a pointer to the value. This pointer's lifetime is
  // tied to the lifetime of the Promise object.
  const T* WaitFor(const MonoDelta& delta) const {
    if (latch_.WaitFor(delta)) {
      return &val_;
    } else {
      return NULL;
    }
  }

  // Set the value of this promise.
  // This may be called at most once.
  void Set(const T& val) {
    DCHECK_EQ(latch_.count(), 1) << "Already set!";
    val_ = val;
    latch_.CountDown();
  }

 private:
  CountDownLatch latch_;
  T val_;
  DISALLOW_COPY_AND_ASSIGN(Promise);
};

} // namespace bb
#endif /* BBOY_UTIL_PROMISE_H */
