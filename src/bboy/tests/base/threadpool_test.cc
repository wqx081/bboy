#include "bboy/base/thread/threadpool.h"
#include "bboy/base/thread/thread.h"
#include "bboy/gbase/atomicops.h"
#include "bboy/gbase/bind.h"
#include "bboy/base/sync/countdown_latch.h"
#include "bboy/base/promise.h"
#include "bboy/base/scoped_cleanup.h"

#include <functional>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <boost/smart_ptr/detail/yield_k.hpp>

using std::shared_ptr;

namespace bb {

namespace {
Status BuildMinMaxTestPool(int min_threads,
                           int max_threads,
                           gscoped_ptr<ThreadPool>* pool) {
  return ThreadPoolBuilder("test").set_min_threads(min_threads)
                                  .set_max_threads(max_threads)
                                  .Build(pool);
}
} // namespace

TEST(TestThreadPool, TestNoTaskOpenClose) {
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(BuildMinMaxTestPool(4, 4, &thread_pool));
  thread_pool->Shutdown();
}

static void SimpleTaskMethod(int n, Atomic32* counter) {
  while (n--) {
    base::subtle::NoBarrier_AtomicIncrement(counter, 1);
    boost::detail::yield(n);
  }
}

class SimpleTask : public Runnable {
 public:
  SimpleTask(int n, Atomic32 *counter)
    : n_(n), counter_(counter) {
  }

  virtual void Run() override {
    SimpleTaskMethod(n_, counter_);
  }

 private:
  int n_;
  Atomic32 *counter_;
};

TEST(TestThreadPool, TestSimpleTasks) {
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(BuildMinMaxTestPool(4, 4, &thread_pool));

  Atomic32 counter(0);
  std::shared_ptr<Runnable> task(new SimpleTask(15, &counter));

  DCHECK_OK(thread_pool->SubmitFunc(std::bind(&SimpleTaskMethod, 10, &counter)));
  DCHECK_OK(thread_pool->Submit(task));
  DCHECK_OK(thread_pool->SubmitFunc(std::bind(&SimpleTaskMethod, 20, &counter)));
  DCHECK_OK(thread_pool->Submit(task));
  DCHECK_OK(thread_pool->SubmitClosure(Bind(&SimpleTaskMethod, 123, &counter)));
  thread_pool->Wait();
  ASSERT_EQ(10 + 15 + 20 + 15 + 123, base::subtle::NoBarrier_Load(&counter));
  thread_pool->Shutdown();
}

static void IssueTraceStatement() {}

TEST(TestThreadPool, TestSubmitAfterShutdown) {
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(BuildMinMaxTestPool(1, 1, &thread_pool));
  thread_pool->Shutdown();
  Status s = thread_pool->SubmitFunc(&IssueTraceStatement);
  ASSERT_EQ("Service unavailable: The pool has been shut down.",
                        s.ToString());
}

class SlowTask : public Runnable {
 public:
  explicit SlowTask(CountDownLatch* latch)
    : latch_(latch) {
  }

  virtual void Run() override {
    latch_->Wait();
  }

 private:
  CountDownLatch* latch_;
};

TEST(TestThreadPool, TestThreadPoolWithNoMinimum) {
  MonoDelta idle_timeout = MonoDelta::FromMilliseconds(1);
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_min_threads(0).set_max_threads(3)
                  .set_idle_timeout(idle_timeout).Build(&thread_pool));
  ASSERT_TRUE(thread_pool->num_threads_ == 0);
  CountDownLatch latch(1);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(2, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(3, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(3, thread_pool->num_threads_);
  latch.CountDown();
  thread_pool->Wait();
  ASSERT_EQ(0, thread_pool->active_threads_);
  thread_pool->Shutdown();
  ASSERT_EQ(0, thread_pool->num_threads_);
} 

TEST(TestThreadPool, TestRace) {
  alarm(60);
  auto cleanup = MakeScopedCleanup([]() {
    alarm(0); // Disable alarm on test exit.
  });
  MonoDelta idle_timeout = MonoDelta::FromMicroseconds(1);
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_min_threads(0).set_max_threads(1)
                  .set_idle_timeout(idle_timeout).Build(&thread_pool));

  for (int i = 0; i < 500; i++) {
    CountDownLatch l(1);
    DCHECK_OK(thread_pool->SubmitFunc(std::bind(static_cast<void(CountDownLatch::*)()>(&CountDownLatch::CountDown), &l)));
    l.Wait();
    SleepFor(MonoDelta::FromMicroseconds(i));
  }
}

TEST(TestThreadPool, TestVariableSizeThreadPool) {
  MonoDelta idle_timeout = MonoDelta::FromMilliseconds(1);
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_min_threads(1).set_max_threads(4)
                  .set_idle_timeout(idle_timeout).Build(&thread_pool));
  ASSERT_EQ(1, thread_pool->num_threads_);
  CountDownLatch latch(1);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(1, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(2, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(3, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(4, thread_pool->num_threads_);
  DCHECK_OK(thread_pool->Submit(
        shared_ptr<Runnable>(new SlowTask(&latch))));
  ASSERT_EQ(4, thread_pool->num_threads_);
  latch.CountDown();
  thread_pool->Wait();
  ASSERT_EQ(0, thread_pool->active_threads_);
  thread_pool->Shutdown();
  ASSERT_EQ(0, thread_pool->num_threads_);
} 

TEST(TestThreadPool, TestMaxQueueSize) {
  gscoped_ptr<ThreadPool> thread_pool; 
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_min_threads(1).set_max_threads(1)
                  .set_max_queue_size(1).Build(&thread_pool));
      
  CountDownLatch latch(1);
  DCHECK_OK(thread_pool->Submit(shared_ptr<Runnable>(new SlowTask(&latch))));
  DCHECK_OK(thread_pool->Submit(shared_ptr<Runnable>(new SlowTask(&latch))));
  Status s = thread_pool->Submit(shared_ptr<Runnable>(new SlowTask(&latch)));
  CHECK(s.IsServiceUnavailable()) << "Expected failure due to queue blowout:" << s.ToString();
  latch.CountDown();
  thread_pool->Wait();
  thread_pool->Shutdown();
}

TEST(TestThreadPool, TestZeroQueueSize) {
  gscoped_ptr<ThreadPool> thread_pool;
  const int kMaxThreads = 4;
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_max_queue_size(0)
                  .set_max_threads(kMaxThreads)
                  .Build(&thread_pool));

  CountDownLatch latch(1);
  for (int i = 0; i < kMaxThreads; i++) {
    DCHECK_OK(thread_pool->Submit(shared_ptr<Runnable>(new SlowTask(&latch))));
  }
  Status s = thread_pool->Submit(shared_ptr<Runnable>(new SlowTask(&latch)));
  ASSERT_TRUE(s.IsServiceUnavailable()) << s.ToString();
  EXPECT_TRUE(s.ToString().find("Thread pool is at capacity") != std::string::npos);
  latch.CountDown();
  thread_pool->Wait();
  thread_pool->Shutdown();
}

TEST(TestThreadPool, TestPromises) {
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(ThreadPoolBuilder("test")
                  .set_min_threads(1).set_max_threads(1)
                  .set_max_queue_size(1).Build(&thread_pool));

  Promise<int> my_promise;
  DCHECK_OK(thread_pool->SubmitClosure(
                     Bind(&Promise<int>::Set, Unretained(&my_promise), 5)));
  ASSERT_EQ(5, my_promise.Get());
  thread_pool->Shutdown();
}

class SlowDestructorRunnable : public Runnable {
 public:
  void Run() override {}

  virtual ~SlowDestructorRunnable() {
    SleepFor(MonoDelta::FromMilliseconds(100));
  }
};

TEST(TestThreadPool, TestSlowDestructor) {
  gscoped_ptr<ThreadPool> thread_pool;
  DCHECK_OK(BuildMinMaxTestPool(1, 20, &thread_pool));
  MonoTime start = MonoTime::Now();
  for (int i = 0; i < 100; i++) {
    shared_ptr<Runnable> task(new SlowDestructorRunnable());
    DCHECK_OK(thread_pool->Submit(std::move(task)));
  }
  thread_pool->Wait();
  ASSERT_LT((MonoTime::Now() - start).ToSeconds(), 5);
}

TEST(TestThreadPool, TestDeadlocks) {
  const char* death_msg = "called pool function that would result in deadlock";
  ASSERT_DEATH({
    gscoped_ptr<ThreadPool> thread_pool;
    DCHECK_OK(ThreadPoolBuilder("test")
                            .set_min_threads(1)
                            .set_max_threads(1)
                            .Build(&thread_pool));
    DCHECK_OK(thread_pool->SubmitClosure(
        Bind(&ThreadPool::Shutdown, Unretained(thread_pool.get()))));
    thread_pool->Wait();
  }, death_msg);

  ASSERT_DEATH({
    gscoped_ptr<ThreadPool> thread_pool;
    DCHECK_OK(ThreadPoolBuilder("test")
                            .set_min_threads(1)
                            .set_max_threads(1)
                            .Build(&thread_pool));
    DCHECK_OK(thread_pool->SubmitClosure(
        Bind(&ThreadPool::Wait, Unretained(thread_pool.get()))));
    thread_pool->Wait();
  }, death_msg);
}


} // namespace bb
