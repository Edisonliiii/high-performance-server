#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <limits.h>
#include <time.h>
// stl
#include <algorithm>
#include <vector>
// test header
#include "../include/io_buffer.h"

namespace {

// class for time calculation
class QuickTest : public ::testing::Test {
  protected:
    time_t start_time_;

    void SetUp() override {start_time_ = time(nullptr);}
    void TearDown() override {
      const time_t end_time = time(nullptr);
      EXPECT_TRUE(end_time - start_time_ <= 5) << "The test took too long.";
    }
}; // QuickTest

// 继承自io_buffer
// 在不影响原class的情况下添加测试方法
class io_buffer_testobj : public io_buffer {
  public:
    // c'tor
    //io_buffer_testobj(){};
    io_buffer_testobj(int size) : io_buffer(size) {}
    // capacity's getter & setter
    int get_capacity() {
      return this->capacity;
    }
    void set_capacity(int _cap) {
      this->capacity = _cap;
    }
    // return length
    int get_length() {
      return this->length;
    }
    void set_length(int _len) {
      this->length = _len;
    }
    // return head
    int get_head() {
      return this->head;
    }
    void set_head(int _hea) {
      this->head = _hea;
    }
    // return next
    io_buffer* get_next() {
      return this->next;
    }
    void set_next(io_buffer* _nex) {
      this->next = _nex;
    }
}; // io_buffer_testobj

// fixture
class IOBufferTEST : public QuickTest{
  protected:
    io_buffer_testobj test = io_buffer_testobj(10);
}; // IOBufferTEST

TEST_F(IOBufferTEST, clearTest) {
  test.set_length(10);
  test.set_head(10);
  test.clear();
  EXPECT_EQ(test.get_length(), 0);
  EXPECT_EQ(test.get_head(), 0);
}

}; // namespace

// main part
#if GTEST_OS_ESP8266 || GTEST_OS_ESP32

#if GTEST_OS_ESP8266
extern "C" {
#endif

void setup() {
   ::testing::InitGoogleTest();
}
void loop() {RUN_ALL_TESTS(); }

#if GTEST_OS_ESP8266
}
#endif

#else
GTEST_API_ int main(int argc, char **argv)
{
    printf("Running main() from %s\n", __FILE__);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif