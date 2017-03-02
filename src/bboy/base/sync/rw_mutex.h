#pragma once

#include <pthread.h>
#include <unordered_set>

#include "bboy/gbase/macros.h"
//#include "bboy/base/sync/locks.h"

namespace bb {

class RWMutex {
 public:

  // Possible fairness policies for the RWMutex.
  enum class Priority {
    // The lock will prioritize readers at the expense of writers.
    PREFER_READING,

    // The lock will prioritize writers at the expense of readers.
    //
    // Care should be taken when using this fairness policy, as it can lead to
    // unexpected deadlocks (e.g. a writer waiting on the lock will prevent
    // additional readers from acquiring it).
    PREFER_WRITING,
  };

  // Create an RWMutex that prioritizes readers.
  RWMutex();

  // Create an RWMutex with customized priority. This is a best effort; the
  // underlying platform may not support custom priorities.
  explicit RWMutex(Priority prio);

  ~RWMutex();

  void ReadLock();
  void ReadUnlock();
  bool TryReadLock();

  void WriteLock();
  void WriteUnlock();
  bool TryWriteLock();

  void AssertAcquiredForReading() const {}
  void AssertAcquiredForWriting() const {}

  // Aliases for use with std::lock_guard and kudu::shared_lock.
  void lock() { WriteLock(); }
  void unlock() { WriteUnlock(); }
  bool try_lock() { return TryWriteLock(); }
  void lock_shared() { ReadLock(); }
  void unlock_shared() { ReadUnlock(); }
  bool try_lock_shared() { return TryReadLock(); }

 private:
  void Init(Priority prio);

  enum class LockState {
    NEITHER,
    READER,
    WRITER,
  };

  void CheckLockState(LockState state) const {}
  void MarkForReading() {}
  void MarkForWriting() {}
  void UnmarkForReading() {}
  void UnmarkForWriting() {}

  pthread_rwlock_t native_handle_;

  DISALLOW_COPY_AND_ASSIGN(RWMutex);
};

} // namespace bb
