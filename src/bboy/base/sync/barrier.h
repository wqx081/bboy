#pragma once

#include "bboy/gbase/macros.h"
#include "bboy/base/sync/condition_variable.h"
#include "bboy/base/sync/mutex.h"

namespace bb {

// Implementation of pthread-style Barriers.
class Barrier {
 public:
  // Initialize the barrier with the given initial count.
  explicit Barrier(int count) :
      cond_(&mutex_),
      count_(count),
      initial_count_(count) {
    DCHECK_GT(count, 0);
  }

  ~Barrier();

  // Wait until all threads have reached the barrier.
  // Once all threads have reached the barrier, the barrier is reset
  // to the initial count.
  void Wait() {
//    ThreadRestrictions::AssertWaitAllowed();
    MutexLock l(mutex_);
    if (--count_ == 0) {
      count_ = initial_count_;
      cycle_count_++;
      cond_.Broadcast();
      return;
    }

    int initial_cycle = cycle_count_;
    while (cycle_count_ == initial_cycle) {
      cond_.Wait();
    }
  }

 private:
  Mutex mutex_;
  ConditionVariable cond_;
  int count_;
  uint32_t cycle_count_ = 0;
  const int initial_count_;
  DISALLOW_COPY_AND_ASSIGN(Barrier);
};

} // namespace bb
