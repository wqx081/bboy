#ifndef BBOY_BASE_MONOTIME_H_
#define BBOY_BASE_MONOTIME_H_

#include <stdint.h>
#include <string>

#include <gtest/gtest_prod.h>

struct timeval;
struct timespec;

namespace bb {

class MonoTime;

class MonoDelta {
 public:
  static MonoDelta FromSeconds(double seconds);
  static MonoDelta FromMilliseconds(int64_t ms);
  static MonoDelta FromMicroseconds(int64_t us);
  static MonoDelta FromNanoseconds(int64_t ns);

  MonoDelta();

  bool Initialized() const;

  bool LessThan(const MonoDelta &rhs) const;

  bool MoreThan(const MonoDelta &rhs) const;

  bool Equals(const MonoDelta &rhs) const;

  std::string ToString() const;

  double ToSeconds() const;
  int64_t ToMilliseconds() const;
  int64_t ToMicroseconds() const;
  int64_t ToNanoseconds() const;

  void ToTimeVal(struct timeval *tv) const;

  void ToTimeSpec(struct timespec *ts) const;

  static void NanosToTimeSpec(int64_t nanos, struct timespec* ts);

 private:
  static const int64_t kUninitialized;

  friend class MonoTime;
  FRIEND_TEST(TestMonoTime, TestDeltaConversions);

  explicit MonoDelta(int64_t delta);
  int64_t nano_delta_;
};

class MonoTime {
 public:
  static const int64_t kNanosecondsPerSecond = 1000000000L;
  static const int64_t kNanosecondsPerMillisecond = 1000000L;
  static const int64_t kNanosecondsPerMicrosecond = 1000L;

  static const int64_t kMicrosecondsPerSecond = 1000000L;

  static MonoTime Now();

  static MonoTime Max();

  static MonoTime Min();

  static const MonoTime& Earliest(const MonoTime& a, const MonoTime& b);

  MonoTime();

  bool Initialized() const;

  MonoDelta GetDeltaSince(const MonoTime &rhs) const;

  void AddDelta(const MonoDelta &delta);

  bool ComesBefore(const MonoTime &rhs) const;

  std::string ToString() const;

  bool Equals(const MonoTime& other) const;

  MonoTime& operator+=(const MonoDelta& delta);

  MonoTime& operator-=(const MonoDelta& delta);

 private:
  friend class MonoDelta;
  FRIEND_TEST(TestMonoTime, TestTimeSpec);
  FRIEND_TEST(TestMonoTime, TestDeltaConversions);

  explicit MonoTime(const struct timespec &ts);
  explicit MonoTime(int64_t nanos);
  double ToSeconds() const;
  int64_t nanos_;
};

void SleepFor(const MonoDelta& delta);

bool operator==(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator!=(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator<(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator<=(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator>(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator>=(const MonoDelta &lhs, const MonoDelta &rhs);

bool operator==(const MonoTime& lhs, const MonoTime& rhs);

bool operator!=(const MonoTime& lhs, const MonoTime& rhs);

bool operator<(const MonoTime& lhs, const MonoTime& rhs);

bool operator<=(const MonoTime& lhs, const MonoTime& rhs);

bool operator>(const MonoTime& lhs, const MonoTime& rhs);

bool operator>=(const MonoTime& lhs, const MonoTime& rhs);

MonoTime operator+(const MonoTime& t, const MonoDelta& delta);

MonoTime operator-(const MonoTime& t, const MonoDelta& delta);

MonoDelta operator-(const MonoTime& t_end, const MonoTime& t_begin);

} // namespace bb
#endif
