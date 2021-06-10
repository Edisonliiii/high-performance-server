#ifndef __EVENT_LOOP_H__
#define __EVENT_LOOP_H__

#include "event_base.h"
#include "timer_queue.h"
#include <sys/epoll.h>
#include <ext/hash_map>
#include <ext/hash_set>

#define MAXEVENTS 10

class event_loop
{
public:
  event_loop();
  void process_evs();
  
  /** operator for IO events **/
  void add_ioev(int fd, io_callback* proc, int mask, void* args = NULL);
  // delete only mask event for fd in epoll
  void del_ioev(int fd, int mask);
  // delete event for fd in epoll
  void del_ioev(int fd);
  // get all fds this loop is listening
  void nlistenings(__gnu_cxx::hash_set<int>& conns) {conns = listening;}

  /** operator for timer event **/
  int run_at(timer_callback cb, void* args, uint64_t ts);
  int run_after(timer_callback cb, void* args, int sec, int millis=0);
  int run_every(timer_callback cb, void* args, int sec, int millis=0);
  void del_timer(int timer_id);
  void add_task(pendingFunc, void* args);
  void run_task();
/********/
  int _io_evs_size() {return _io_evs.size();}
private:
  /* 总体来说时钟事件最终还是被当做一个io时间来处理 
     一开始初始化 时钟事件就会被加入到io_evs当中去
  */
  int _epfd;
  /* [epoll_event]
     struct epoll_event
     {
       uint32_t events;         // the type of epoll events like EPOLLIN, EPOLLOUT ... 
       epoll_data_t data;       // user data variable, for usr space usage, to reserve context
     };

     typedef union epoll_data{
       void*      ptr;
       int        fd;
       uint32_t   u32;
       uint64_t   u64;
     } epoll_data_t;
  */
  struct epoll_event _fired_evs[MAXEVENTS];                            // 可用事件队列
  __gnu_cxx::hash_map<int, io_event> _io_evs;                          // (fd, io_event) 存着user space定义的各种"IO"事件, 属于待跑队列
  typedef __gnu_cxx::hash_map<int, io_event>::iterator ioev_it;        // _io_evs iterator

  timer_queue* _timer_queue;                                           // 存着user space定义的各种将要去执行的timer任务
  std::vector<std::pair<pendingFunc, void*>> _pendingFactors;          // (callback, args)
  __gnu_cxx::hash_set<int> listening;                                  // hash set of fd from user space, listened by kernel epoll

  friend void timerqueue_cb(event_loop* loop, int fd, void* args);
};

#endif











