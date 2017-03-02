#ifndef BBOY_BASE_SYNC_COUNTDOWN_LATCH_H
#define BBOY_BASE_SYNC_COUNTDOWN_LATCH_H

#include "bboy/gbase/macros.h"
#include "bboy/base/sync/condition_variable.h"
#include "bboy/base/sync/mutex.h"

#include "bboy/base/monotime.h"

namespace bb {

class CountDownLatch {
 public:
  explicit CountDownLatch(int count)
    : cond_(&lock_),
      count_(count) {
  }

  ~CountDownLatch();

  void CountDown(int amount) {
    DCHECK_GE(amount, 0);
    MutexLock lock(lock_);
    if (count_ == 0) {
      return;
    }

    if (amount >= count_) {
      count_ = 0;
    } else {
      count_ -= amount;
    }

    if (count_ == 0) {
      // Latch has triggered.
      cond_.Broadcast();
    }
  }

  void CountDown() {
    CountDown(1);
  }

  void Wait() const {
//    ThreadRestrictions::AssertWaitAllowed();
    MutexLock lock(lock_);
    while (count_ > 0) {
      cond_.Wait();
    }
  }

  bool WaitUntil(const MonoTime& when) const {
//    ThreadRestrictions::AssertWaitAllowed();
    return WaitFor(when - MonoTime::Now());
  }

  bool WaitFor(const MonoDelta& delta) const {
//    ThreadRestrictions::AssertWaitAllowed();
    MutexLock lock(lock_);
    while (count_ > 0) {
      if (!cond_.TimedWait(delta)) {
        return false;
      }
    }
    return true;
  }

  void Reset(uint64_t count) {
    MutexLock lock(lock_);
    count_ = count;
    if (count_ == 0) {
      // Awake any waiters if we reset to 0.
      cond_.Broadcast();
    }
  }

  uint64_t count() const {
    MutexLock lock(lock_);
    return count_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CountDownLatch);
  mutable Mutex lock_;
  ConditionVariable cond_;

  uint64_t count_;
};

// Utility class which calls latch->CountDown() in its destructor.
class CountDownOnScopeExit {
 public:
  explicit CountDownOnScopeExit(CountDownLatch *latch) : latch_(latch) {}
  ~CountDownOnScopeExit() {
    latch_->CountDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CountDownOnScopeExit);

  CountDownLatch *latch_;
};

} // namespace kudu
#endif
