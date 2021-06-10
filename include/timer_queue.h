#ifndef __TIMER_QUEUE_H__
#define __TIMER_QUEUE_H__

#include <stdint.h>
#include <vector>
#include <ext/hash_map>
#include <iostream>
#include "event_base.h"

class timer_queue
{
private:
//data
  // 时钟事件vector
  std::vector<timer_event> _event_lst;
  typedef std::vector<timer_event>::iterator vit;
  
  // (timer_id, index_in_event_lst)
  __gnu_cxx::hash_map<int, int> _position;
  typedef __gnu_cxx::hash_map<int, int>::iterator mit;

  int _count;           // 当前timer在_event_lst中的id
  int _next_timer_id;   // 全局记录timer_event的 从小到大增长
  int _timerfd;         // posix per-process timer, is a fd
                        // timer和kernel之间通过这个fd来传递信号决定是不是过期了 决定是否重置
  uint64_t _pioneer;    // recent timer's millis 总记录最小的时间长度
                        // -1 when uninitialized (empty heap)
//member
  void reset_timo();
  void heap_add(timer_event& te);
  void heap_del(int pos);
  void heap_pop();
  void heap_hold(int pos);
public:
  // c'tor
  timer_queue();                                          // arm a timer by creating a new for _timerfd
  // c c'tor
  timer_queue(timer_queue&& _other);
  // assgin c'tor
  timer_queue& operator=(timer_queue&& _other);
  ~timer_queue();                                         // disarm the timer by accessing fd

  int add_timer(timer_event& te);
  void del_timer(int timer_id);
  int notifier() const { return _timerfd; }               // return _timerfd
  int size() const { return _count; }                     // return cur size of event_lst
  void get_timo(std::vector<timer_event>& fired_evs);

  // ------------------------------------ for test ---------------------------------------------------
  void print_event_lst() {
  	for (int i=0; i<_event_lst.size(); ++i){
  	  std::cout<<_event_lst[i].ts<<" ";
  	}
  	std::cout<<std::endl;
  }
  uint64_t& returnPioneer()                                {return _pioneer;}
  std::vector<timer_event>* returnEventLst()               {return (std::vector<timer_event>*)&_event_lst;}
  __gnu_cxx::hash_map<int, int>* returnPosition()          {return (__gnu_cxx::hash_map<int, int>*)&_position;}
  int& returnNextTimerID()                                 {return _next_timer_id;}
  void call_heap_add(timer_event& te)                      {heap_add(te);}
  void call_heap_del(int pos)                              {heap_del(pos);}
  void call_heap_pop()                                     {heap_pop();}
  void call_heap_hold(int pos)                             {heap_hold(pos);}
  void call_get_timo(std::vector<timer_event>& fired_evs)  {get_timo(fired_evs);}
};

#endif



