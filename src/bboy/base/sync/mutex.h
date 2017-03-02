#ifndef BBOY_BASE_SYNC_MUTEX_H_
#define BBOY_BASE_SYNC_MUTEX_H_

#include <memory>
#include <string>
#include <glog/logging.h>

#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/macros.h"

namespace bb {

class Mutex {
 public:
  Mutex();
  ~Mutex();

  void Acquire();
  void Release();
  bool TryAcquire();

  void lock() { Acquire(); }
  void unlock() { Release(); }
  bool try_lock() { return TryAcquire(); }

  void AssertAcquired() const {}

 private:
  friend class ConditionVariable;

  pthread_mutex_t native_handle_;

  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class MutexLock {
 public:
  struct AlreadyAcquired {};

  explicit MutexLock(Mutex& lock)
    : lock_(&lock),
      owned_(true) {
    lock_->Acquire();
  }

  MutexLock(Mutex& lock, const AlreadyAcquired&)
    : lock_(&lock),
      owned_(true) {
    lock_->AssertAcquired();
  }

  void Lock() {
    DCHECK(!owned_);
    lock_->Acquire();
    owned_ = true;
  }

  void Unlock() {
    DCHECK(owned_);
    lock_->AssertAcquired();
    lock_->Release();
    owned_ = false;
  }

  ~MutexLock() {
    if (owned_) {
      Unlock();
    }
  }

  bool OwnsLock() const { return owned_; }

 private:
  Mutex* lock_;
  bool owned_;
  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

} // namespace bb
#endif // BBOY_BASE_SYNC_MUTEX_H_
