#ifndef BBOY_BASE_SYNC_CONDITION_VARIABLE_H_
#define BBOY_BASE_SYNC_CONDITION_VARIABLE_H_

#include "bboy/base/sync/mutex.h"
#include "bboy/base/monotime.h"

namespace bb {

class TimeDelta;

class ConditionVariable {
 public:
  explicit ConditionVariable(Mutex* user_lock);
  ~ConditionVariable();

  void Wait() const;
  bool TimedWait(const MonoDelta& wait_time) const;
  
  void Broadcast();
  void Signal();

 private:
  DISALLOW_COPY_AND_ASSIGN(ConditionVariable);

  mutable pthread_cond_t native_condition_;
  pthread_mutex_t* user_mutex_;
};

} // namespace bb
#endif // BBOY_BASE_SYNC_CONDITION_VARIABLE_H_
