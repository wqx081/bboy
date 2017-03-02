#include "bboy/base/thread/thread.h"

#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include <sys/prctl.h>



#include "bboy/gbase/atomicops.h"
#include "bboy/gbase/dynamic_annotations.h"
#include "bboy/gbase/once.h"
#include "bboy/gbase/mathlimits.h"
#include "bboy/gbase/strings/substitute.h"
#include "bboy/base/os-util.h"
#include "bboy/base/errno.h"

#include <boost/smart_ptr/detail/yield_k.hpp>

using std::bind;
using std::mem_fn;
using std::endl;
using std::map;
using std::ostringstream;
using std::shared_ptr;
using strings::Substitute;

namespace bb {

#if 0
static uint64_t GetCpuUTime() {
  rusage ru;
  CHECK_ERR(getrusage(RUSAGE_SELF, &ru));
  return ru.ru_utime.tv_sec * 1000UL + ru.ru_utime.tv_usec / 1000UL;
}

static uint64_t GetCpuSTime() {
  rusage ru;
  CHECK_ERR(getrusage(RUSAGE_SELF, &ru));
  return ru.ru_stime.tv_sec * 1000UL + ru.ru_stime.tv_usec / 1000UL;
}

static uint64_t GetVoluntaryContextSwitches() {
  rusage ru;
  CHECK_ERR(getrusage(RUSAGE_SELF, &ru));
  return ru.ru_nvcsw;;
}

static uint64_t GetInVoluntaryContextSwitches() {
  rusage ru;
  CHECK_ERR(getrusage(RUSAGE_SELF, &ru));
  return ru.ru_nivcsw;
}
#endif

class ThreadMgr;

__thread Thread* Thread::tls_ = NULL;

static shared_ptr<ThreadMgr> thread_manager;

// Controls the single (lazy) initialization of thread_manager.
static GoogleOnceType once = GOOGLE_ONCE_INIT;

// A singleton class that tracks all live threads, and groups them together for easy
// auditing. Used only by Thread.
class ThreadMgr {
 public:
  ThreadMgr()
      : metrics_enabled_(false),
        threads_started_metric_(0),
        threads_running_metric_(0) {
  }

  ~ThreadMgr() {
    MutexLock l(lock_);
    thread_categories_.clear();
  }

  static void SetThreadName(const std::string& name, int64 tid);

//  Status StartInstrumentation(const scoped_refptr<MetricEntity>& metrics, WebCallbackRegistry* web);

  void AddThread(const pthread_t& pthread_id, const string& name, const string& category,
      int64_t tid);

  void RemoveThread(const pthread_t& pthread_id, const string& category);

 private:
  class ThreadDescriptor {
   public:
    ThreadDescriptor() { }
    ThreadDescriptor(string category, string name, int64_t thread_id)
        : name_(std::move(name)),
          category_(std::move(category)),
          thread_id_(thread_id) {}

    const string& name() const { return name_; }
    const string& category() const { return category_; }
    int64_t thread_id() const { return thread_id_; }

   private:
    string name_;
    string category_;
    int64_t thread_id_;
  };

  typedef map<const pthread_t, ThreadDescriptor> ThreadCategory;

  typedef map<string, ThreadCategory> ThreadCategoryMap;

  Mutex lock_;

  ThreadCategoryMap thread_categories_;

  bool metrics_enabled_;

  uint64_t threads_started_metric_;
  uint64_t threads_running_metric_;

  uint64_t ReadThreadsStarted();
  uint64_t ReadThreadsRunning();

  //void ThreadPathHandler(const WebCallbackRegistry::WebRequest& args, ostringstream* output);
  //void PrintThreadCategoryRows(const ThreadCategory& category, ostringstream* output);
};

void ThreadMgr::SetThreadName(const string& name, int64 tid) {
  if (tid == getpid()) {
    return;
  }

  int err = prctl(PR_SET_NAME, name.c_str());
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM) {
    PLOG(ERROR) << "SetThreadName";
  }
}

#if 0
Status ThreadMgr::StartInstrumentation(const scoped_refptr<MetricEntity>& metrics,
                                       WebCallbackRegistry* web) {
  MutexLock l(lock_);
  metrics_enabled_ = true;

  // Use function gauges here so that we can register a unique copy of these metrics in
  // multiple tservers, even though the ThreadMgr is itself a singleton.
  metrics->NeverRetire(
      METRIC_threads_started.InstantiateFunctionGauge(metrics,
        Bind(&ThreadMgr::ReadThreadsStarted, Unretained(this))));
  metrics->NeverRetire(
      METRIC_threads_running.InstantiateFunctionGauge(metrics,
        Bind(&ThreadMgr::ReadThreadsRunning, Unretained(this))));
  metrics->NeverRetire(
      METRIC_cpu_utime.InstantiateFunctionGauge(metrics,
        Bind(&GetCpuUTime)));
  metrics->NeverRetire(
      METRIC_cpu_stime.InstantiateFunctionGauge(metrics,
        Bind(&GetCpuSTime)));
  metrics->NeverRetire(
      METRIC_voluntary_context_switches.InstantiateFunctionGauge(metrics,
        Bind(&GetVoluntaryContextSwitches)));
  metrics->NeverRetire(
      METRIC_involuntary_context_switches.InstantiateFunctionGauge(metrics,
        Bind(&GetInVoluntaryContextSwitches)));

  WebCallbackRegistry::PathHandlerCallback thread_callback =
      bind<void>(mem_fn(&ThreadMgr::ThreadPathHandler), this, _1, _2);
  DCHECK_NOTNULL(web)->RegisterPathHandler("/threadz", "Threads", thread_callback);
  return Status::OK();
}
#endif

uint64_t ThreadMgr::ReadThreadsStarted() {
  MutexLock l(lock_);
  return threads_started_metric_;
}

uint64_t ThreadMgr::ReadThreadsRunning() {
  MutexLock l(lock_);
  return threads_running_metric_;
}

void ThreadMgr::AddThread(const pthread_t& pthread_id, const string& name,
    const string& category, int64_t tid) {
  // These annotations cause TSAN to ignore the synchronization on lock_
  // without causing the subsequent mutations to be treated as data races
  // in and of themselves (that's what IGNORE_READS_AND_WRITES does).
  //
  // Why do we need them here and in SuperviseThread()? TSAN operates by
  // observing synchronization events and using them to establish "happens
  // before" relationships between threads. Where these relationships are
  // not built, shared state access constitutes a data race. The
  // synchronization events here, in RemoveThread(), and in
  // SuperviseThread() may cause TSAN to establish a "happens before"
  // relationship between thread functors, ignoring potential data races.
  // The annotations prevent this from happening.
  ANNOTATE_IGNORE_SYNC_BEGIN();
  ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN();
  {
    MutexLock l(lock_);
    thread_categories_[category][pthread_id] = ThreadDescriptor(category, name, tid);
    if (metrics_enabled_) {
      threads_running_metric_++;
      threads_started_metric_++;
    }
  }
  ANNOTATE_IGNORE_SYNC_END();
  ANNOTATE_IGNORE_READS_AND_WRITES_END();
}

void ThreadMgr::RemoveThread(const pthread_t& pthread_id, const string& category) {
  ANNOTATE_IGNORE_SYNC_BEGIN();
  ANNOTATE_IGNORE_READS_AND_WRITES_BEGIN();
  {
    MutexLock l(lock_);
    auto category_it = thread_categories_.find(category);
    DCHECK(category_it != thread_categories_.end());
    category_it->second.erase(pthread_id);
    if (metrics_enabled_) {
      threads_running_metric_--;
    }
  }
  ANNOTATE_IGNORE_SYNC_END();
  ANNOTATE_IGNORE_READS_AND_WRITES_END();
}

#if 0
void ThreadMgr::PrintThreadCategoryRows(const ThreadCategory& category,
    ostringstream* output) {
  for (const ThreadCategory::value_type& thread : category) {
    ThreadStats stats;
    Status status = GetThreadStats(thread.second.thread_id(), &stats);
    if (!status.ok()) {
      KLOG_EVERY_N(INFO, 100) << "Could not get per-thread statistics: "
                              << status.ToString();
    }
    (*output) << "<tr><td>" << thread.second.name() << "</td><td>"
              << (static_cast<double>(stats.user_ns) / 1e9) << "</td><td>"
              << (static_cast<double>(stats.kernel_ns) / 1e9) << "</td><td>"
              << (static_cast<double>(stats.iowait_ns) / 1e9) << "</td></tr>";
  }
}

void ThreadMgr::ThreadPathHandler(const WebCallbackRegistry::WebRequest& req,
    ostringstream* output) {
  MutexLock l(lock_);
  vector<const ThreadCategory*> categories_to_print;
  auto category_name = req.parsed_args.find("group");
  if (category_name != req.parsed_args.end()) {
    string group = EscapeForHtmlToString(category_name->second);
    (*output) << "<h2>Thread Group: " << group << "</h2>" << endl;
    if (group != "all") {
      ThreadCategoryMap::const_iterator category = thread_categories_.find(group);
      if (category == thread_categories_.end()) {
        (*output) << "Thread group '" << group << "' not found" << endl;
        return;
      }
      categories_to_print.push_back(&category->second);
      (*output) << "<h3>" << category->first << " : " << category->second.size()
                << "</h3>";
    } else {
      for (const ThreadCategoryMap::value_type& category : thread_categories_) {
        categories_to_print.push_back(&category.second);
      }
      (*output) << "<h3>All Threads : </h3>";
    }

    (*output) << "<table class='table table-hover table-border'>";
    (*output) << "<tr><th>Thread name</th><th>Cumulative User CPU(s)</th>"
              << "<th>Cumulative Kernel CPU(s)</th>"
              << "<th>Cumulative IO-wait(s)</th></tr>";

    for (const ThreadCategory* category : categories_to_print) {
      PrintThreadCategoryRows(*category, output);
    }
    (*output) << "</table>";
  } else {
    (*output) << "<h2>Thread Groups</h2>";
    if (metrics_enabled_) {
      (*output) << "<h4>" << threads_running_metric_ << " thread(s) running";
    }
    (*output) << "<a href='/threadz?group=all'><h3>All Threads</h3>";

    for (const ThreadCategoryMap::value_type& category : thread_categories_) {
      string category_arg;
      UrlEncode(category.first, &category_arg);
      (*output) << "<a href='/threadz?group=" << category_arg << "'><h3>"
                << category.first << " : " << category.second.size() << "</h3></a>";
    }
  }
}
#endif

static void InitThreading() {
//  ignore_result(GetStackTraceHex());
  thread_manager.reset(new ThreadMgr());
}

#if 0
Status StartThreadInstrumentation(const scoped_refptr<MetricEntity>& server_metrics,
                                  WebCallbackRegistry* web) {
  GoogleOnceInit(&once, &InitThreading);
  return thread_manager->StartInstrumentation(server_metrics, web);
}
#endif

ThreadJoiner::ThreadJoiner(Thread* thr)
  : thread_(CHECK_NOTNULL(thr)),
    warn_after_ms_(kDefaultWarnAfterMs),
    warn_every_ms_(kDefaultWarnEveryMs),
    give_up_after_ms_(kDefaultGiveUpAfterMs) {
}

ThreadJoiner& ThreadJoiner::warn_after_ms(int ms) {
  warn_after_ms_ = ms;
  return *this;
}

ThreadJoiner& ThreadJoiner::warn_every_ms(int ms) {
  warn_every_ms_ = ms;
  return *this;
}

ThreadJoiner& ThreadJoiner::give_up_after_ms(int ms) {
  give_up_after_ms_ = ms;
  return *this;
}

Status ThreadJoiner::Join() {
  if (Thread::current_thread() &&
      Thread::current_thread()->tid() == thread_->tid()) {
    return Status::InvalidArgument("Can't join on own thread", thread_->name_);
  }

  // Early exit: double join is a no-op.
  if (!thread_->joinable_) {
    return Status::OK();
  }

  int waited_ms = 0;
  bool keep_trying = true;
  while (keep_trying) {
    if (waited_ms >= warn_after_ms_) {
      LOG(WARNING) << Substitute("Waited for $0ms trying to join with $1 (tid $2)",
                                 waited_ms, thread_->name_, thread_->tid_);
    }

    int remaining_before_giveup = MathLimits<int>::kMax;
    if (give_up_after_ms_ != -1) {
      remaining_before_giveup = give_up_after_ms_ - waited_ms;
    }

    int remaining_before_next_warn = warn_every_ms_;
    if (waited_ms < warn_after_ms_) {
      remaining_before_next_warn = warn_after_ms_ - waited_ms;
    }

    if (remaining_before_giveup < remaining_before_next_warn) {
      keep_trying = false;
    }

    int wait_for = std::min(remaining_before_giveup, remaining_before_next_warn);

    if (thread_->done_.WaitFor(MonoDelta::FromMilliseconds(wait_for))) {
      // Unconditionally join before returning, to guarantee that any TLS
      // has been destroyed (pthread_key_create() destructors only run
      // after a pthread's user method has returned).
      int ret = pthread_join(thread_->thread_, NULL);
      CHECK_EQ(ret, 0);
      thread_->joinable_ = false;
      return Status::OK();
    }
    waited_ms += wait_for;
  }
  return Status::Aborted(strings::Substitute("Timed out after $0ms joining on $1",
                                             waited_ms, thread_->name_));
}

Thread::~Thread() {
  if (joinable_) {
    int ret = pthread_detach(thread_);
    CHECK_EQ(ret, 0);
  }
}

void Thread::CallAtExit(const Closure& cb) {
  CHECK_EQ(Thread::current_thread(), this);
  exit_callbacks_.push_back(cb);
}

std::string Thread::ToString() const {
  return Substitute("Thread $0 (name: \"$1\", category: \"$2\")", tid_, name_, category_);
}

Status Thread::StartThread(const std::string& category, const std::string& name,
                           const ThreadFunctor& functor, uint64_t flags,
                           scoped_refptr<Thread> *holder) {
//  TRACE_COUNTER_INCREMENT("threads_started", 1);
//  TRACE_COUNTER_SCOPE_LATENCY_US("thread_start_us");
  const string log_prefix = Substitute("$0 ($1) ", name, category);
//  SCOPED_LOG_SLOW_EXECUTION_PREFIX(WARNING, 500 /* ms */, log_prefix, "starting thread");

  // Temporary reference for the duration of this function.
  scoped_refptr<Thread> t(new Thread(category, name, functor));

  {
//    SCOPED_LOG_SLOW_EXECUTION_PREFIX(WARNING, 500 /* ms */, log_prefix, "creating pthread");
//    SCOPED_WATCH_STACK((flags & NO_STACK_WATCHDOG) ? 0 : 250);
    int ret = pthread_create(&t->thread_, NULL, &Thread::SuperviseThread, t.get());
    if (ret) {
      return Status::RuntimeError("Could not create thread", strerror(ret), ret);
    }
  }

  t->joinable_ = true;

  if (holder) {
    *holder = t;
  }

  Release_Store(&t->tid_, PARENT_WAITING_TID);
  {
//    SCOPED_LOG_SLOW_EXECUTION_PREFIX(WARNING, 500 /* ms */, log_prefix,
//                                     "waiting for new thread to publish its TID");
    int loop_count = 0;
    while (Acquire_Load(&t->tid_) == PARENT_WAITING_TID) {
      boost::detail::yield(loop_count++);
    }
  }

  VLOG(2) << "Started thread " << t->tid()<< " - " << category << ":" << name;
  return Status::OK();
}

void* Thread::SuperviseThread(void* arg) {
  Thread* t = static_cast<Thread*>(arg);
  int64_t system_tid = Thread::CurrentThreadId();
  if (system_tid == -1) {
    string error_msg = ErrnoToString(errno);
//    KLOG_EVERY_N(INFO, 100) << "Could not determine thread ID: " << error_msg;
  }
  string name = strings::Substitute("$0-$1", t->name(), system_tid);

  // Take an additional reference to the thread manager, which we'll need below.
  GoogleOnceInit(&once, &InitThreading);
  ANNOTATE_IGNORE_SYNC_BEGIN();
  shared_ptr<ThreadMgr> thread_mgr_ref = thread_manager;
  ANNOTATE_IGNORE_SYNC_END();

  // Set up the TLS.
  //
  // We could store a scoped_refptr in the TLS itself, but as its
  // lifecycle is poorly defined, we'll use a bare pointer and take an
  // additional reference on t out of band, in thread_ref.
  scoped_refptr<Thread> thread_ref = t;
  t->tls_ = t;

  // Wait until the parent has updated all caller-visible state, then write
  // the TID to 'tid_', thus completing the parent<-->child handshake.
  int loop_count = 0;
  while (Acquire_Load(&t->tid_) == CHILD_WAITING_TID) {
    boost::detail::yield(loop_count++);
  }
  Release_Store(&t->tid_, system_tid);

  thread_manager->SetThreadName(name, t->tid());
  thread_manager->AddThread(pthread_self(), name, t->category(), t->tid());

  // FinishThread() is guaranteed to run (even if functor_ throws an
  // exception) because pthread_cleanup_push() creates a scoped object
  // whose destructor invokes the provided callback.
  pthread_cleanup_push(&Thread::FinishThread, t);
  t->functor_();
  pthread_cleanup_pop(true);

  return NULL;
}

void Thread::FinishThread(void* arg) {
  Thread* t = static_cast<Thread*>(arg);

  for (Closure& c : t->exit_callbacks_) {
    c.Run();
  }

  // We're here either because of the explicit pthread_cleanup_pop() in
  // SuperviseThread() or through pthread_exit(). In either case,
  // thread_manager is guaranteed to be live because thread_mgr_ref in
  // SuperviseThread() is still live.
  thread_manager->RemoveThread(pthread_self(), t->category());

  // Signal any Joiner that we're done.
  t->done_.CountDown();

  VLOG(2) << "Ended thread " << t->tid() << " - "
          << t->category() << ":" << t->name();
}

} // namespace kudu
