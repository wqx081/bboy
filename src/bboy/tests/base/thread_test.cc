#include "bboy/base/thread/thread.h"

#include <gtest/gtest.h>

#include <string>

#include "bboy/gbase/ref_counted.h"

namespace bb {

class ThreadTest : public testing::Test { };

TEST_F(ThreadTest, TestCtor) {
  scoped_refptr<Thread> thread_holder;

  DCHECK_OK(Thread::Create("test_catagory", "test_thread_name", []() {
    LOG(INFO) << "Run at thread: " << Thread::UniqueThreadId();
  }, &thread_holder));
  thread_holder->Join();
  LOG(INFO) << "Run at thread: " << Thread::UniqueThreadId();
}

TEST_F(ThreadTest, TestJoinAndWarn) {

  scoped_refptr<Thread> holder;
  DCHECK_OK(Thread::Create("test", "sleeper thread", usleep, 1000*1000, &holder));
  DCHECK_OK(ThreadJoiner(holder.get())
            .warn_after_ms(10)
            .warn_every_ms(100)
            .Join());
}

TEST_F(ThreadTest, TestFailedJoin) {

  scoped_refptr<Thread> holder;
  DCHECK_OK(Thread::Create("test", "sleeper thread", usleep, 1000*1000, &holder));
  Status s = ThreadJoiner(holder.get())
    .give_up_after_ms(50)
    .Join();
  EXPECT_TRUE(s.ToString().find("Timed out after 50ms joining on sleeper thread") != std::string::npos);
}

static void TryJoinOnSelf() {
  Status s = ThreadJoiner(Thread::current_thread()).Join();
  CHECK(s.IsInvalidArgument());
}

TEST_F(ThreadTest, TestJoinOnSelf) {
  scoped_refptr<Thread> holder;
  DCHECK_OK(Thread::Create("test", "test", TryJoinOnSelf, &holder));
  holder->Join();
}

TEST_F(ThreadTest, TestDoubleJoinIsNoOp) {
  scoped_refptr<Thread> holder;
  DCHECK_OK(Thread::Create("test", "sleeper thread", usleep, 0, &holder));
  ThreadJoiner joiner(holder.get());
  DCHECK_OK(joiner.Join());
  DCHECK_OK(joiner.Join());
}

namespace {

void ExitHandler(std::string* s, const char* to_append) {
  *s += to_append;
}

void CallAtExitThread(std::string* s) {
  Thread::current_thread()->CallAtExit(Bind(&ExitHandler, s, Unretained("hello 1, ")));
  Thread::current_thread()->CallAtExit(Bind(&ExitHandler, s, Unretained("hello 2")));
}

} // anonymous namespace

TEST_F(ThreadTest, TestCallOnExit) {
  scoped_refptr<Thread> holder;
  std::string s;
  DCHECK_OK(Thread::Create("test", "TestCallOnExit", CallAtExitThread, &s, &holder));
  holder->Join();
  ASSERT_EQ("hello 1, hello 2", s);
}

} // namespace
