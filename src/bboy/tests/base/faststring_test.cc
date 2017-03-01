#include <gtest/gtest.h>
#include <glog/logging.h>

#include "bboy/base/faststring.h"

namespace bb {

TEST(faststring, Ctor) {
  faststring str;
  EXPECT_TRUE(str.length() == 0);
  EXPECT_TRUE(str.size() == 0);
  EXPECT_TRUE(str.capacity() == 32);

  str.reserve(30);
  EXPECT_TRUE(str.capacity() == 32);
  str.reserve(42);
  EXPECT_TRUE(str.capacity() == 42);
}

TEST(faststring, Append) {
  faststring str;
  str.append("1234567890");
  EXPECT_TRUE(str.length() == 10);
  str.append("1234567890");
  str.append("1234567890");
  str.append("1234567890");
  EXPECT_TRUE(str.length() == 40);

  std::string s = "1234567890";
  s += s;
  s += s;

  LOG(INFO) << "str: " << str.ToString();
  LOG(INFO) << "  s: " << s;
  EXPECT_TRUE(str.ToString() == s);
}

TEST(faststring, Release) {
  faststring str;
  uint8_t* r = str.release();
  delete[] r;
  EXPECT_TRUE(str.length() == 0);
  EXPECT_TRUE(str.capacity() == 32);

}

TEST(faststring, PushBack) {
  faststring str;
  str.push_back('a');
  EXPECT_TRUE(str.length() == 1);
  str.push_back('b');
  EXPECT_TRUE(str.length() == 2);
}

TEST(faststring, Access) {
  faststring str;
  str.push_back('h');
  str.push_back('i');

  EXPECT_EQ('h', str[0]);
  EXPECT_EQ('i', str[1]);
}

} // namespace bb
