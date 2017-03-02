#include "bboy/base/sync/rw_mutex.h"

#include <glog/logging.h>
#include <mutex>

#include "bboy/gbase/map-util.h"

using std::lock_guard;

namespace {

void unlock_rwlock(pthread_rwlock_t* rwlock) {
  int rv = pthread_rwlock_unlock(rwlock);
  DCHECK_EQ(0, rv) << strerror(rv);
}

} // anonymous namespace

namespace bb {

RWMutex::RWMutex() {
  Init(Priority::PREFER_READING);
}

RWMutex::RWMutex(Priority prio) {
  Init(prio);
}

void RWMutex::Init(Priority prio) {
  // Adapt from priority to the pthread type.
  int kind = PTHREAD_RWLOCK_PREFER_READER_NP;
  switch (prio) {
    case Priority::PREFER_READING:
      kind = PTHREAD_RWLOCK_PREFER_READER_NP;
      break;
    case Priority::PREFER_WRITING:
      kind = PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP;
      break;
  }

  // Initialize the new rwlock with the user's preference.
  pthread_rwlockattr_t attr;
  int rv = pthread_rwlockattr_init(&attr);
  DCHECK_EQ(0, rv) << strerror(rv);
  rv = pthread_rwlockattr_setkind_np(&attr, kind);
  DCHECK_EQ(0, rv) << strerror(rv);
  rv = pthread_rwlock_init(&native_handle_, &attr);
  DCHECK_EQ(0, rv) << strerror(rv);
  rv = pthread_rwlockattr_destroy(&attr);
  DCHECK_EQ(0, rv) << strerror(rv);
}

RWMutex::~RWMutex() {
  int rv = pthread_rwlock_destroy(&native_handle_);
  DCHECK_EQ(0, rv) << strerror(rv);
}

void RWMutex::ReadLock() {
  CheckLockState(LockState::NEITHER);
  int rv = pthread_rwlock_rdlock(&native_handle_);
  DCHECK_EQ(0, rv) << strerror(rv);
  MarkForReading();
}

void RWMutex::ReadUnlock() {
  CheckLockState(LockState::READER);
  UnmarkForReading();
  unlock_rwlock(&native_handle_);
}

bool RWMutex::TryReadLock() {
  CheckLockState(LockState::NEITHER);
  int rv = pthread_rwlock_tryrdlock(&native_handle_);
  if (rv == EBUSY) {
    return false;
  }
  DCHECK_EQ(0, rv) << strerror(rv);
  MarkForReading();
  return true;
}

void RWMutex::WriteLock() {
  CheckLockState(LockState::NEITHER);
  int rv = pthread_rwlock_wrlock(&native_handle_);
  DCHECK_EQ(0, rv) << strerror(rv);
  MarkForWriting();
}

void RWMutex::WriteUnlock() {
  CheckLockState(LockState::WRITER);
  UnmarkForWriting();
  unlock_rwlock(&native_handle_);
}

bool RWMutex::TryWriteLock() {
  CheckLockState(LockState::NEITHER);
  int rv = pthread_rwlock_trywrlock(&native_handle_);
  if (rv == EBUSY) {
    return false;
  }
  DCHECK_EQ(0, rv) << strerror(rv);
  MarkForWriting();
  return true;
}

} // namespace kudu
