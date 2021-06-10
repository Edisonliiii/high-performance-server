#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <limits.h>
#include <time.h>
// stl
#include <algorithm>
#include <vector>
// needed resources within project
#include "../include/event_loop.h"

namespace {
static int _fd;
// timing a test in a test fixture
class QuickTest : public ::testing::Test {
  protected:
    time_t start_time_;

    void SetUp() override {start_time_ = time(nullptr);}
    void TearDown() override {
      const time_t end_time = time(nullptr);
      EXPECT_TRUE(end_time - start_time_ <= 5) << "The test took too long.";
    }
}; // QuickTest

class MockAddIOev : public QuickTest {
protected:
  event_loop el;
//  enum class mask_set {EPOLLIN, EPOLLOUT};
//  enum class opcodes_set {EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD};
//  io_event* returnNewEvn(int fd, io_callback* proc=NULL, int mask=EPOLLIN, void* args=NULL) {
//    return new io_event(_fd++, nullptr, EPOLLIN, nullptr);
//  }
}; // MockAddIOev


}; // namespace

TEST_F(MockAddIOev, ADDIOevTest) {
  el.add_ioev(_fd++, nullptr, EPOLLIN, nullptr);
  EXPECT_EQ(2, el._io_evs_size());
}

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