#include "bboy/base/sync/condition_variable.h"

#include <glog/logging.h>

#include "bboy/base/errno.h"

namespace bb {

ConditionVariable::ConditionVariable(Mutex* user_lock)
  : user_mutex_(&user_lock->native_handle_) {
  int rv = 0;
  pthread_condattr_t attrs;
  rv = pthread_condattr_init(&attrs);
  DCHECK_EQ(0, rv);  
  pthread_condattr_setclock(&attrs, CLOCK_MONOTONIC);
  rv = pthread_cond_init(&native_condition_, &attrs);
  pthread_condattr_destroy(&attrs);
  DCHECK_EQ(0, rv);
}

ConditionVariable::~ConditionVariable() {
  int rv = pthread_cond_destroy(&native_condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Wait() const {
  int rv = pthread_cond_wait(&native_condition_, user_mutex_);
  DCHECK_EQ(0, rv);
}

bool ConditionVariable::TimedWait(const MonoDelta& wait_time) const {

  int64 nsecs = wait_time.ToNanoseconds();
  if (nsecs < 0) {
    return false;
  }

  struct timespec relative_time;
  wait_time.ToTimeSpec(&relative_time);

  struct timespec absolute_time;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  absolute_time.tv_sec = now.tv_sec;
  absolute_time.tv_nsec = now.tv_nsec;

  absolute_time.tv_sec += relative_time.tv_sec;
  absolute_time.tv_nsec += relative_time.tv_nsec;
  absolute_time.tv_sec += absolute_time.tv_nsec / MonoTime::kNanosecondsPerSecond;
  absolute_time.tv_nsec %= MonoTime::kNanosecondsPerSecond;
  DCHECK_GE(absolute_time.tv_sec, now.tv_sec);

  int rv = pthread_cond_timedwait(&native_condition_, user_mutex_, &absolute_time);
  DCHECK(rv == 0 || rv == ETIMEDOUT) 
      << "unexpected pthread_cond_timedwait return value: " << rv;
  return rv == 0;
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&native_condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&native_condition_);
  DCHECK_EQ(0, rv);
}

} // namespace bb
