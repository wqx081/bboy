#ifndef BBOY_BASE_SYNC_LOCKS_H
#define BBOY_BASE_SYNC_LOCKS_H

#include <algorithm>
#include <glog/logging.h>
#include <mutex>

#include "bboy/gbase/atomicops.h"
#include "bboy/gbase/dynamic_annotations.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/port.h"
#include "bboy/gbase/spinlock.h"
#include "bboy/gbase/sysinfo.h"

#include "bboy/base/sync/rw_semaphore.h"

namespace bb {

using base::subtle::Acquire_CompareAndSwap;
using base::subtle::NoBarrier_Load;
using base::subtle::Release_Store;

class simple_spinlock {
 public:
  simple_spinlock() {}

  void lock() {
    l_.Lock();
  }

  void unlock() {
    l_.Unlock();
  }

  bool try_lock() {
    return l_.TryLock();
  }

  bool is_locked() {
    return l_.IsHeld();
  }

 private:
  base::SpinLock l_;

  DISALLOW_COPY_AND_ASSIGN(simple_spinlock);
};

struct padded_spinlock : public simple_spinlock {
  char padding[CACHELINE_SIZE - (sizeof(simple_spinlock) % CACHELINE_SIZE)];
};

class rw_spinlock {
 public:
  rw_spinlock() {
    ANNOTATE_RWLOCK_CREATE(this);
  }
  ~rw_spinlock() {
    ANNOTATE_RWLOCK_DESTROY(this);
  }

  void lock_shared() {
    sem_.lock_shared();
    ANNOTATE_RWLOCK_ACQUIRED(this, 0);
  }

  void unlock_shared() {
    ANNOTATE_RWLOCK_RELEASED(this, 0);
    sem_.unlock_shared();
  }

  bool try_lock() {
    bool ret = sem_.try_lock();
    if (ret) {
      ANNOTATE_RWLOCK_ACQUIRED(this, 1);
    }
    return ret;
  }

  void lock() {
    sem_.lock();
    ANNOTATE_RWLOCK_ACQUIRED(this, 1);
  }

  void unlock() {
    ANNOTATE_RWLOCK_RELEASED(this, 1);
    sem_.unlock();
  }

  bool is_write_locked() const {
    return sem_.is_write_locked();
  }

  bool is_locked() const {
    return sem_.is_locked();
  }

 private:
  rw_semaphore sem_;
};

class percpu_rwlock {
 public:
  percpu_rwlock() {
    n_cpus_ = base::MaxCPUIndex() + 1;
    CHECK_GT(n_cpus_, 0);
    locks_ = new padded_lock[n_cpus_];
  }

  ~percpu_rwlock() {
    delete [] locks_;
  }

  rw_spinlock &get_lock() {
    int cpu = sched_getcpu();
    CHECK_LT(cpu, n_cpus_);
    return locks_[cpu].lock;
  }

  bool try_lock() {
    for (int i = 0; i < n_cpus_; i++) {
      if (!locks_[i].lock.try_lock()) {
        while (i--) {
          locks_[i].lock.unlock();
        }
        return false;
      }
    }
    return true;
  }

  bool is_locked() const {
    for (int i = 0; i < n_cpus_; i++) {
      if (locks_[i].lock.is_locked()) return true;
    }
    return false;
  }

  void lock() {
    for (int i = 0; i < n_cpus_; i++) {
      locks_[i].lock.lock();
    }
  }

  void unlock() {
    for (int i = 0; i < n_cpus_; i++) {
      locks_[i].lock.unlock();
    }
  }

  size_t memory_footprint_excluding_this() const;

  size_t memory_footprint_including_this() const;

 private:
  struct padded_lock {
    rw_spinlock lock;
    char padding[CACHELINE_SIZE - (sizeof(rw_spinlock) % CACHELINE_SIZE)];
  };

  int n_cpus_;
  padded_lock *locks_;
};

template <typename Mutex>
class shared_lock {
 public:
  shared_lock()
      : m_(nullptr) {
  }

  explicit shared_lock(Mutex& m)
      : m_(&m) {
    m_->lock_shared();
  }

  shared_lock(Mutex& m, std::try_to_lock_t t)
      : m_(nullptr) {
    if (m.try_lock_shared()) {
      m_ = &m;
    }
  }

  bool owns_lock() const {
    return m_;
  }

  void swap(shared_lock& other) {
    std::swap(m_,other.m_);
  }

  ~shared_lock() {
    if (m_ != nullptr) {
      m_->unlock_shared();
    }
  }

 private:
  Mutex* m_;
  DISALLOW_COPY_AND_ASSIGN(shared_lock<Mutex>);
};

} // namespace bb
#endif
