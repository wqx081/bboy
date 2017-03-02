#ifndef BBOY_BASE_SYNC_SEMAPHORE_H_
#define BBOY_BASE_SYNC_SEMAPHORE_H_

#include <semaphore.h>

#include "bboy/gbase/macros.h"
#include "bboy/gbase/port.h"
#include "bboy/base/monotime.h"


namespace bb {

class Semaphore {
 public:
  explicit Semaphore(int capacity);
  ~Semaphore();

  void Acquire();

  bool TimedAcquire(const MonoDelta& timeout);

  bool TryAcquire();

  void Release();

  int GetValue();

  void lock() { Acquire(); }
  void unlock() { Release(); }
  bool try_lock() { return TryAcquire(); }

 private:
  void Fatal(const char* action) ATTRIBUTE_NORETURN;

  sem_t sem_;
  DISALLOW_COPY_AND_ASSIGN(Semaphore);
};

} // namespace bb
#endif /* BBOY_BASE_SYNC_SEMAPHORE_H */
