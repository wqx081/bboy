#pragma once

#include "bboy/gbase/bind.h"
#include "bboy/gbase/macros.h"

#include "bboy/base/sync/countdown_latch.h"
#include "bboy/base/status.h"
#include "bboy/base/status_callback.h"

namespace bb {

// Simple class which can be used to make async methods synchronous.
// For example:
//   Synchronizer s;
//   SomeAsyncMethod(s.callback());
//   CHECK_OK(s.Wait());
class Synchronizer {
 public:
  Synchronizer()
    : l(1) {
  }
  void StatusCB(const Status& status) {
    s = status;
    l.CountDown();
  }
  StatusCallback AsStatusCallback() {
    // Synchronizers are often declared on the stack, so it doesn't make
    // sense for a callback to take a reference to its synchronizer.
    //
    // Note: this means the returned callback _must_ go out of scope before
    // its synchronizer.
    return Bind(&Synchronizer::StatusCB, Unretained(this));
  }
  Status Wait() {
    l.Wait();
    return s;
  }
  Status WaitFor(const MonoDelta& delta) {
    if (PREDICT_FALSE(!l.WaitFor(delta))) {
      return Status::TimedOut("Timed out while waiting for the callback to be called.");
    }
    return s;
  }
  void Reset() {
    l.Reset(1);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(Synchronizer);
  Status s;
  CountDownLatch l;
};

} // namespace bb
