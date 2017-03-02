#ifndef BBOY_BASE_THREAD_THREADPOOL_H_
#define BBOY_BASE_THREAD_THREADPOOL_H_

#include <functional>
#include <gtest/gtest_prod.h>
#include <list>
#include <memory>
#include <unordered_set>
#include <string>
#include <vector>

#include "bboy/gbase/callback_forward.h"
#include "bboy/gbase/gscoped_ptr.h"
#include "bboy/gbase/macros.h"
#include "bboy/gbase/port.h"
#include "bboy/gbase/ref_counted.h"

#include "bboy/base/monotime.h"
#include "bboy/base/status.h"

#include "bboy/base/sync/mutex.h"
#include "bboy/base/sync/condition_variable.h"

namespace bb {

class Thread;
class ThreadPool;

// @pattern  Command pattern
class Runnable {
 public:
  virtual void Run() = 0;
  virtual ~Runnable() {}
};

// @pattern Builder pattern
class ThreadPoolBuilder {
 public:
  explicit ThreadPoolBuilder(std::string name);

  ThreadPoolBuilder& set_min_threads(int min_threads);
  ThreadPoolBuilder& set_max_threads(int max_threads);
  ThreadPoolBuilder& set_max_queue_size(int max_queue_size);
  ThreadPoolBuilder& set_idle_timeout(const MonoDelta& idle_timeout);

  const std::string& name() const { return name_; }
  int min_threads() const { return min_threads_; }
  int max_threads() const { return max_threads_; }
  int max_queue_size() const { return max_queue_size_; }
  const MonoDelta& idle_timeout() const { return idle_timeout_; }

  Status Build(gscoped_ptr<ThreadPool>* pool) const;

 private:
  friend class ThreadPool;
  const std::string name_;
  // metric_prefix_
  int min_threads_;
  int max_threads_;
  int max_queue_size_;
  MonoDelta idle_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolBuilder);
};

class ThreadPool {
 public:
  ~ThreadPool();

  void Shutdown();

  Status SubmitClosure(const Closure& task) WARN_UNUSED_RESULT;
  Status SubmitFunc(std::function<void()> func) WARN_UNUSED_RESULT;
  Status Submit(std::shared_ptr<Runnable> task) WARN_UNUSED_RESULT;

  void Wait();
  bool WaitUntil(const MonoTime& until);
  bool WaitFor(const MonoDelta& delta);

  int queue_length() const { return queue_size_; }

 private:
  friend class ThreadPoolBuilder;
  explicit ThreadPool(const ThreadPoolBuilder& builder);

  Status Init();
  void DispatchThread(bool permanent);
  Status CreateThreadUnlocked();
  void CheckNotPoolThreadUnlocked();

 private:
  FRIEND_TEST(TestThreadPool, TestThreadPoolWithNoMinimum);
  FRIEND_TEST(TestThreadPool, TestVariableSizeThreadPool);

  struct QueueEntry {
    std::shared_ptr<Runnable> runnable;
    // Trace* trace;

    MonoTime sumbit_time;
  };

  const std::string name_;
  const int min_threads_;
  const int max_threads_;
  const int max_queue_size_;
  const MonoDelta idle_timeout_;

  Status pool_status_;
  Mutex lock_;
  ConditionVariable idle_cond_;
  ConditionVariable no_threads_cond_;
  ConditionVariable not_empty_;
  int num_threads_;
  int active_threads_;
  int queue_size_;
  std::list<QueueEntry> queue_;

  std::unordered_set<Thread*> threads_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

} // namespace bb
#endif // BBOY_BASE_THREAD_THREADPOOL_H_
