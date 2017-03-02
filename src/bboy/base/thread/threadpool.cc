#include "bboy/base/thread/threadpool.h"

#include "bboy/base/thread/thread.h"

#include "bboy/gbase/stl_util.h"
#include "bboy/gbase/map-util.h"
#include "bboy/gbase/callback.h"
#include "bboy/gbase/strings/substitute.h"
#include "bboy/gbase/sysinfo.h"
#include "bboy/gbase/walltime.h"

namespace bb {

using strings::Substitute;

// FunctionRunnable
class FunctionRunnable : public Runnable {
 public:
  explicit FunctionRunnable(std::function<void()> func) 
    : func_(std::move(func)) {}

  virtual void Run() override {
    func_();
  }

 private:
  std::function<void()> func_;
};

// ThreadPoolBuilder
ThreadPoolBuilder::ThreadPoolBuilder(std::string name)
  : name_(std::move(name)),
    min_threads_(0),
    max_threads_(base::NumCPUs()),
    max_queue_size_(std::numeric_limits<int>::max()),
    idle_timeout_(MonoDelta::FromMilliseconds(500)) {}

ThreadPoolBuilder& ThreadPoolBuilder::set_min_threads(int min_threads) {
  CHECK_GE(min_threads, 0);
  min_threads_ = min_threads;
  return *this;
}

ThreadPoolBuilder& ThreadPoolBuilder::set_max_threads(int max_threads) {
  CHECK_GT(max_threads, 0);
  max_threads_ = max_threads;
  return *this;
}

ThreadPoolBuilder& ThreadPoolBuilder::set_max_queue_size(int max_queue_size) {
  max_queue_size_ = max_queue_size;
  return *this;
}

ThreadPoolBuilder& ThreadPoolBuilder::set_idle_timeout(const MonoDelta& idle_timeout) {
  idle_timeout_ = idle_timeout;
  return *this;
}

Status ThreadPoolBuilder::Build(gscoped_ptr<ThreadPool>* pool) const {
  pool->reset(new ThreadPool(*this));
  RETURN_NOT_OK((*pool)->Init());
  return Status::OK();
}

// ThreadPool
ThreadPool::ThreadPool(const ThreadPoolBuilder& builder)
  : name_(builder.name_),
    min_threads_(builder.min_threads_),
    max_threads_(builder.max_threads_),
    max_queue_size_(builder.max_queue_size_),
    idle_timeout_(builder.idle_timeout_),
    pool_status_(Status::Uninitialized("The pool was not initialized.")),
    idle_cond_(&lock_),
    no_threads_cond_(&lock_),
    not_empty_(&lock_),
    num_threads_(0),
    active_threads_(0),
    queue_size_(0) {
}

ThreadPool::~ThreadPool() {
  Shutdown();
}

Status ThreadPool::Init() {
  MutexLock unique_lock(lock_);
  if (!pool_status_.IsUninitialized()) {
    return Status::NotSupported("The thread pool is already initialized");
  }
  pool_status_ = Status::OK();
  // Create min_threads_ Thread
  for (int i = 0; i < min_threads_; i++) {
    Status status = CreateThreadUnlocked();
    if (!status.ok()) {
      Shutdown();
      return status;
    }
  }
  return Status::OK();
}

void ThreadPool::Shutdown() {
  MutexLock unique_lock(lock_);
  CheckNotPoolThreadUnlocked();

  pool_status_ = Status::ServiceUnavailable("The pool has been shut down.");

  auto to_release = std::move(queue_);
  queue_.clear();
  queue_size_ = 0;
  not_empty_.Broadcast();

  while (num_threads_ > 0) {
    no_threads_cond_.Wait();
  }

  unique_lock.Unlock();

  // TODO(wqx):
#if 0
  for (QueueEntry& e : to_release) {
    // e.trace->Release();
  }
#endif
}

Status ThreadPool::SubmitClosure(const Closure& task) {
  return SubmitFunc(std::bind(&Closure::Run, task));
}

Status ThreadPool::SubmitFunc(std::function<void()> func) {
  return Submit(std::shared_ptr<Runnable>(new FunctionRunnable(std::move(func))));
}

Status ThreadPool::Submit(std::shared_ptr<Runnable> task) {

  MonoTime submit_time = MonoTime::Now();

  MutexLock guard(lock_);
  if (PREDICT_FALSE(!pool_status_.ok())) {
    return pool_status_;
  }

  int64_t capacity_remaining = static_cast<int64_t>(max_threads_) - active_threads_ +
                               static_cast<int64_t>(max_queue_size_) - queue_size_;

  if (capacity_remaining < 1) {
    return Status::ServiceUnavailable(
            Substitute("Thread pool is at capacity ($0/$1 tasks running, $2/$3 tasks queued)",
                       num_threads_, max_threads_, queue_size_, max_queue_size_));
  }

  int inactive_threads = num_threads_ - active_threads_;
  int additional_threads = (queue_size_ + 1) - inactive_threads;
  if (additional_threads > 0 && num_threads_ < max_threads_) {
    Status status = CreateThreadUnlocked();
    if (!status.ok()) {
      if (num_threads_ == 0) {
        return status;
      }

      LOG(ERROR) << "Thread pool failed to create thread: " << status.ToString();
    }
  }

  QueueEntry e;
  e.runnable = std::move(task);
  // e.trace
  e.sumbit_time = submit_time;

  queue_.emplace_back(std::move(e));
  queue_size_++;

  guard.Unlock();
  not_empty_.Signal();

  return Status::OK();
}

void ThreadPool::Wait() {
  MutexLock unique_lock(lock_);
  CheckNotPoolThreadUnlocked();
  while ((!queue_.empty()) || (active_threads_ > 0)) {
    idle_cond_.Wait();
  }
}

bool ThreadPool::WaitUntil(const MonoTime& until) {
  return WaitFor(until - MonoTime::Now());
}

bool ThreadPool::WaitFor(const MonoDelta& delta) {
  MutexLock unique_lock(lock_);
  CheckNotPoolThreadUnlocked();
  while ((!queue_.empty()) || (active_threads_ > 0)) {
    if (!idle_cond_.TimedWait(delta)) {
      return false;
    }
  }
  return true;
}

void ThreadPool::DispatchThread(bool permanent) {
  MutexLock unique_lock(lock_);
  while (true) {
    if (!pool_status_.ok()) {
      VLOG(2) << "DispatchThread exiting: " << pool_status_.ToString();
      break;
    }

    if (queue_.empty()) {
      if (permanent) {
        not_empty_.Wait();
      } else {
        if (!not_empty_.TimedWait(idle_timeout_)) {
          if (queue_.empty()) {
            VLOG(3) << "Releasing worker thread from pool " << name_ << " after "
                    << idle_timeout_.ToMilliseconds() << " ms of idle time.";
            break;
          }
        }
      }
      continue;
    }

    // Fetch pending task
    QueueEntry entry = std::move(queue_.front());
    queue_.pop_front();
    queue_size_--;
    ++active_threads_;

    unique_lock.Unlock();

    // Execute the task
    {
      MicrosecondsInt64 start_wall_us = GetMonoTimeMicros();
      MicrosecondsInt64 start_cpu_us = GetThreadCpuTimeMicros();

      entry.runnable->Run();

      int64_t wall_us = GetMonoTimeMicros() - start_wall_us;
      int64_t cpu_us = GetThreadCpuTimeMicros() - start_cpu_us;

      VLOG(2) << "task: need wall_us: " << wall_us << ", cpu_us: " << cpu_us;
    }

    entry.runnable.reset();
    unique_lock.Lock();

    if (--active_threads_ == 0) {
      idle_cond_.Broadcast();
    }
  } // end loop

  CHECK(unique_lock.OwnsLock());

  CHECK_EQ(threads_.erase(Thread::current_thread()), 1);
  if (--num_threads_ == 0) {
    no_threads_cond_.Broadcast();
    CHECK(queue_.empty());
    DCHECK_EQ(0, queue_size_);
  }
}

Status ThreadPool::CreateThreadUnlocked() {
  bool permanent = (num_threads_ < min_threads_);
  scoped_refptr<Thread> t;
  Status s =    Thread::Create("thread pool",
                                strings::Substitute("$0 [worker]", name_),
                                &ThreadPool::DispatchThread,
                                this,
                                permanent,
                                &t);
  if (s.ok()) {
    InsertOrDie(&threads_, t.get());
    num_threads_++;
  }
  return Status::OK();
}

void ThreadPool::CheckNotPoolThreadUnlocked() {
  Thread* current = Thread::current_thread();
  if (ContainsKey(threads_, current)) {
    LOG(FATAL) << Substitute("Thread belonging to thread pool '$0' with "
                             "name '$1' called pool function that would result in deadlock",
                              name_, current->name());
  }
}

} // namespace bb
