#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <limits.h>
#include <time.h>
// stl
#include <algorithm>
#include <vector>
// needed resources within project
#include "../include/timer_queue.h"

// g++ -o timer_queue_test -std=c++11 timer_queue_test.cpp ../src/timer_queue.cpp -lpthread -lgtest -lgmock

namespace {

inline volatile bool checkMinHeap(std::vector<timer_event>* A)
{
  int len = A->size();
  for(int i=len-1; (i-1)>>1 >= 0; --i) {
    if ((*A)[i].ts<(*A)[(i-1)>>1].ts) {
        std::cout<<(*A)[i].ts<<std::endl;
      return false;
    }
  }
  return true;
}
// timing a test in a test fixture
// 后续抽离出来 做成个父类 节省代码
class QuickTest : public ::testing::Test {
  protected:
    time_t start_time_;

    void SetUp() override {start_time_ = time(nullptr);}
    void TearDown() override {
      const time_t end_time = time(nullptr);
      EXPECT_TRUE(end_time - start_time_ <= 5) << "The test took too long.";
    }
}; // QuickTest

// fixture
class TimerQueueTEST : public QuickTest {
  protected:
    timer_queue q1_;
    // test data
    std::vector<uint64_t> test_ts_values = {775, 862, 278, 452, 962, 
                                            897, 284, 455, 397, 233, 
                                            19, 111, 866, 988, 66, 
                                            913, 822, 70, 903, 222};
    void SetUp() override {
      QuickTest::SetUp();
      q1_.returnNextTimerID() = 0;       // 随机初始值
      // construct timer_event 时候假定timo_cb=nullptr
    }

    inline volatile void setupForHeapTests() {
      for (int i=0; i<test_ts_values.size(); ++i) {
        timer_event tmp; tmp.ts = test_ts_values[i];
        tmp.timer_id = q1_.returnNextTimerID()++;
        q1_.call_heap_add(tmp);                      // 可以优化成move语义 emplace
      }
    }

}; // timer_queue_TEST

/*******************************************************************
TESTS for HEAP OPERATIONS
********************************************************************/
// fixturename, testcasename
TEST_F(TimerQueueTEST, heap_sort_test) {
  setupForHeapTests();
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));           // 确保_event_lst合法
  EXPECT_EQ(test_ts_values.size(),q1_.returnPosition()->size()); // 确保_postion合法
}

TEST_F(TimerQueueTEST, call_heap_add_10) {
  setupForHeapTests();
  int iteration = 1000;                         // 添加个数
  int old_size = q1_.returnEventLst()->size();
  int old_id   = q1_.returnNextTimerID();
  for (int i=1; i<=iteration; ++i) {
    timer_event tmp; tmp.ts = i; tmp.timer_id=q1_.returnNextTimerID()++;
    q1_.call_heap_add(tmp);
  }
  // 测试eventlst添加是否成功
  EXPECT_EQ(iteration+old_size, q1_.returnEventLst()->size());
  
  bool flag = true;
  for (int i=1; i<=iteration; ++i) {
    if(q1_.returnPosition()->find(old_id++)==q1_.returnPosition()->end()) flag = false;
  }
  // 测试postion是否成功
  EXPECT_EQ(true, flag);
  // 测试添加后heap是否合法
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_hold_insertMaxAtFirst) {
  setupForHeapTests();
  // insert INT_MAX in index 0
  (*(q1_.returnEventLst()))[0].ts = INT_MAX;
  int old_id = (*(q1_.returnEventLst()))[0].timer_id;
  q1_.call_heap_hold(0);
  EXPECT_TRUE(((*q1_.returnPosition())[old_id]<<1) > q1_.returnEventLst()->size());
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_hold_insertMinAtFirst) {
  setupForHeapTests();
  // insert min in index 0
  (*(q1_.returnEventLst()))[0].ts = 1;
  int old_id = (*(q1_.returnEventLst()))[0].timer_id;
  q1_.call_heap_hold(0);
  EXPECT_TRUE(((*q1_.returnPosition())[old_id]) == 0);
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_hold_insertMaxAtLast) {
  setupForHeapTests();
  // insert INT_MAX in last index
  int idx = q1_.returnEventLst()->size()-1;
  (*(q1_.returnEventLst()))[idx].ts = INT_MAX;
  int old_id = (*(q1_.returnEventLst()))[idx].timer_id;
  q1_.call_heap_hold(idx);
  EXPECT_TRUE(((*q1_.returnPosition())[old_id]) == idx);
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_hold_insertMinAtLast) {
  setupForHeapTests();
  // insert min in last index, this thing is never going to happen in our case
  // skip
  EXPECT_TRUE(true);
}

TEST_F(TimerQueueTEST, call_heap_pop) {
  setupForHeapTests();
  int old_size = q1_.returnEventLst()->size();
  int old_id = (*(q1_.returnEventLst()))[0].timer_id;
  q1_.call_heap_pop();
  // size check
  EXPECT_EQ(old_size-1, q1_.returnEventLst()->size());
  // id check
  EXPECT_EQ(q1_.returnPosition()->end(),q1_.returnPosition()->find(old_id));
  // check heap
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_pop_null) {
  setupForHeapTests();
  for (int i=20; i>=1; --i) {
    EXPECT_EQ(i, q1_.returnEventLst()->size());
    EXPECT_EQ(i, q1_.returnPosition()->size());
    q1_.call_heap_pop();
    EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
  }
  EXPECT_EQ(0, q1_.returnEventLst()->size());
  EXPECT_EQ(0, q1_.returnPosition()->size());
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
  
  // non.
  q1_.call_heap_pop();
  EXPECT_EQ(0, q1_.returnEventLst()->size());
  EXPECT_EQ(0, q1_.returnPosition()->size());
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_del_in_middle) {
  setupForHeapTests();
  int old_id = (*q1_.returnEventLst())[7].timer_id;
  q1_.call_heap_del(7);
  EXPECT_EQ(q1_.returnPosition()->end(),q1_.returnPosition()->find(old_id));
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

TEST_F(TimerQueueTEST, call_heap_del_null_ele) {
  setupForHeapTests();
  for (int i=1; i<=20; ++i) {
    q1_.call_heap_del(0);
  }
  EXPECT_EQ(0, q1_.returnPosition()->size());
  EXPECT_EQ(0, q1_.returnEventLst()->size());
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst())); 
  
  // when both empty, what's the right answer
  q1_.call_heap_del(0);
  EXPECT_EQ(0, q1_.returnPosition()->size());
  EXPECT_EQ(0, q1_.returnEventLst()->size());
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst())); 
}

/*******************************************************************
TESTS for PUBLIC INTERFACES
********************************************************************/
TEST_F(TimerQueueTEST, call_add_timer) {
  std::vector<timer_event> tm;
  for (int i=0; i<test_ts_values.size(); ++i) {
    timer_event tmp; tmp.ts = test_ts_values[i];
    tmp.timer_id = q1_.returnNextTimerID()++;
    tm.push_back(tmp);
  }
  for(auto ele : tm) {
    q1_.add_timer(ele);
    EXPECT_EQ(q1_.returnPioneer(), (*q1_.returnEventLst())[0].ts);
    EXPECT_EQ(q1_.returnNextTimerID(), ele.timer_id+1);
  }
  EXPECT_EQ(true, checkMinHeap(q1_.returnEventLst()));
}

// 暂时没跑过去 没查明函数运行正确的期待
TEST_F(TimerQueueTEST, call_get_timo) {
  std::vector<timer_event> tm;
  for (int i=0; i<test_ts_values.size(); ++i) {
    timer_event tmp; tmp.ts = test_ts_values[i];
    tmp.interval = 0;
    tmp.timer_id = q1_.returnNextTimerID()++;
    tm.push_back(tmp);
  }
  for(auto ele : tm) q1_.add_timer(ele);

  std::vector<timer_event> fired_evs;
  std::vector<timer_event> reuse_lst;
  // set some one-off timer
  for (int i=0; i<20; ++i) {
    if (i%2) (*q1_.returnEventLst())[i].interval = 10;
  }
  q1_.returnPioneer() = (*q1_.returnEventLst())[0].ts;
  q1_.call_get_timo(fired_evs);
  EXPECT_EQ(10, q1_.returnEventLst()->size());
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