#ifndef BBOY_BASE_SYNC_RW_SEMAPHORE_H_
#define BBOY_BASE_SYNC_RW_SEMAPHORE_H_

#include <boost/smart_ptr/detail/yield_k.hpp>
#include <glog/logging.h>

#include "bboy/gbase/atomicops.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/port.h"

namespace bb {

class rw_semaphore {
 public:
  rw_semaphore() : state_(0) {
  }
  ~rw_semaphore();

  void lock_shared() {
    int loop_count = 0;
    Atomic32 cur_state = base::subtle::NoBarrier_Load(&state_);
    while (true) {
      Atomic32 expected = cur_state & kNumReadersMask;   // I expect no write lock
      Atomic32 try_new_state = expected + 1;          // Add me as reader
      cur_state = base::subtle::Acquire_CompareAndSwap(&state_, expected, try_new_state);
      if (cur_state == expected)
        break;
      // Either was already locked by someone else, or CAS failed.
      boost::detail::yield(loop_count++);
    }
  }

  void unlock_shared() {
    int loop_count = 0;
    Atomic32 cur_state = base::subtle::NoBarrier_Load(&state_);
    while (true) {
      DCHECK_GT(cur_state & kNumReadersMask, 0)
        << "unlock_shared() called when there are no shared locks held";
      Atomic32 expected = cur_state;           // I expect a write lock and other readers
      Atomic32 try_new_state = expected - 1;   // Drop me as reader
      cur_state = base::subtle::Release_CompareAndSwap(&state_, expected, try_new_state);
      if (cur_state == expected)
        break;
      // Either was already locked by someone else, or CAS failed.
      boost::detail::yield(loop_count++);
    }
  }

  // Tries to acquire a write lock, if no one else has it.
  // This function retries on CAS failure and waits for readers to complete.
  bool try_lock() {
    int loop_count = 0;
    Atomic32 cur_state = base::subtle::NoBarrier_Load(&state_);
    while (true) {
      // someone else has already the write lock
      if (cur_state & kWriteFlag)
        return false;

      Atomic32 expected = cur_state & kNumReadersMask;   // I expect some 0+ readers
      Atomic32 try_new_state = kWriteFlag | expected;    // I want to lock the other writers
      cur_state = base::subtle::Acquire_CompareAndSwap(&state_, expected, try_new_state);
      if (cur_state == expected)
        break;
      // Either was already locked by someone else, or CAS failed.
      boost::detail::yield(loop_count++);
    }

    WaitPendingReaders();
    RecordLockHolderStack();
    return true;
  }

  void lock() {
    int loop_count = 0;
    Atomic32 cur_state = base::subtle::NoBarrier_Load(&state_);
    while (true) {
      Atomic32 expected = cur_state & kNumReadersMask;   // I expect some 0+ readers
      Atomic32 try_new_state = kWriteFlag | expected;    // I want to lock the other writers
      // Note: we use NoBarrier here because we'll do the Acquire barrier down below
      // in WaitPendingReaders
      cur_state = base::subtle::NoBarrier_CompareAndSwap(&state_, expected, try_new_state);
      if (cur_state == expected)
        break;
      // Either was already locked by someone else, or CAS failed.
      boost::detail::yield(loop_count++);
    }

    WaitPendingReaders();

    RecordLockHolderStack();
  }

  void unlock() {
    // I expect to be the only writer
    DCHECK_EQ(base::subtle::NoBarrier_Load(&state_), kWriteFlag);

    ResetLockHolderStack();
    // Reset: no writers & no readers.
    Release_Store(&state_, 0);
  }

  // Return true if the lock is currently held for write by any thread.
  // See simple_semaphore::is_locked() for details about where this is useful.
  bool is_write_locked() const {
    return base::subtle::NoBarrier_Load(&state_) & kWriteFlag;
  }

  // Return true if the lock is currently held, either for read or write
  // by any thread.
  // See simple_semaphore::is_locked() for details about where this is useful.
  bool is_locked() const {
    return base::subtle::NoBarrier_Load(&state_);
  }

 private:
  static const uint32_t kNumReadersMask = 0x7fffffff;
  static const uint32_t kWriteFlag = 1 << 31;

  void RecordLockHolderStack() {
  }
  void ResetLockHolderStack() {
  }

  void WaitPendingReaders() {
    int loop_count = 0;
    while ((base::subtle::Acquire_Load(&state_) & kNumReadersMask) > 0) {
      boost::detail::yield(loop_count++);
    }
  }

 private:
  volatile Atomic32 state_;
};

} // namespace bb
#endif /* BBOY_BASE_SYNC_RW_SEMAPHORE_H */
