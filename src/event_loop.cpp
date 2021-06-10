#include "../include/event_loop.h"
#include "../include/print_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* 率先执行_event_lst中heap顶端的timer
 */
void timerqueue_cb(event_loop* loop, int fd, void* args)
{
  std::vector<timer_event> _local_fired_evs;
  loop->_timer_queue->get_timo(_local_fired_evs);
  for (std::vector<timer_event>::iterator itr = _local_fired_evs.begin();
       itr != _local_fired_evs.end(); ++itr)
  {
    itr->cb(loop, itr->cb_data);
  }
}

/* c'tor
*/
event_loop::event_loop()
{
  _epfd = ::epoll_create1(0);                                                 // return file descriptor referring to the new epoll instance
  exit_if(_epfd == -1, "epoll_create1() failed!");                            // 所有的这部分都要升级成RAII
  _timer_queue = new timer_queue();                                           // 一个event_loop对应一个timer_queue 
  exit_if(_timer_queue == NULL, "timer_queue() failed!");
  add_ioev(_timer_queue->notifier(), timerqueue_cb, EPOLLIN, _timer_queue);   //register timer event to event loop
}

/**********************
operations for IO event 
***********************/
void event_loop::process_evs()
{
  while(true)
  {
    ioev_it itr;
    /* epoll_wait:
       waits for events on the epoll instance referred to by the fd
       (here is _epfd)

       the memo area pointed to by events will contain the events that will be available for the caller
       (here is _fired_evs)

       timeout specifies the mini number of milliseconds that epoll_wait will block
       (will be rounded up to the system clock granularity by kernel)

       upto maxevnets are returned by epoll_wait(), should be greater than zero]

       return user space ready epitem objs
    */
    int nfds = ::epoll_wait(_epfd, _fired_evs, MAXEVENTS, 10);
    
    for (int i=0; i<nfds; ++i)                             // go through all nfds (从头到尾 一个不落？)
    {
      itr = _io_evs.find(_fired_evs[i].data.fd);           // if in _fired_evs
      assert(itr != _io_evs.end());
      io_event* ev = &(itr->second);                       // points to _io_evs
      if (_fired_evs[i].events & EPOLLIN)                  // read only
      {
        void* args = ev->rcb_args;                          // config read args
        ev->read_cb(this, _fired_evs[i].data.fd, args);     // config read cb
      } else if (_fired_evs[i].events & EPOLLOUT){         // writable
        void* args = ev->wcb_args;
        ev->write_cb(this, _fired_evs[i].data.fd, args);
      } else if (_fired_evs[i].events & (EPOLLHUP | EPOLLERR)) {
        if (ev->read_cb) {
          void* args = ev->rcb_args;
          ev->read_cb(this, _fired_evs[i].data.fd, args);
        } else if (ev->write_cb){
          void* args = ev->wcb_args;
          ev->write_cb(this, _fired_evs[i].data.fd, args);
        } else {
          error_log("fd %d get error, delete it from epoll", _fired_evs[i].data.fd);
          del_ioev(_fired_evs[i].data.fd);
        }
      }
    }
    run_task();
  }
}
/* from _io_evs to listening
*/
void event_loop::add_ioev(int fd, io_callback* proc, int mask, void* args)
{
  int f_mask = 0;                           // initial mask;
  int op;
  ioev_it itr = _io_evs.find(fd);
  // config or modify new io_event
  if (itr == _io_evs.end())
  {
    f_mask = mask;
    op = EPOLL_CTL_ADD;                     // add new _io_evs to epoll
  } else {
    f_mask = itr->second.mask | mask;
    op = EPOLL_CTL_MOD;                     // modify io_evs in epoll
  }
  // config io_event is writable or readable
  if (mask & EPOLLIN) {
    _io_evs[fd].read_cb = proc;
    _io_evs[fd].rcb_args = args;
  } else if(mask & EPOLLOUT) {
    _io_evs[fd].write_cb = proc;
    _io_evs[fd].wcb_args = args;
  }
  
  _io_evs[fd].mask = f_mask;
  // create a new epoll_event obj
  struct epoll_event event;
  event.events = f_mask;                    // define the type of the events
  event.data.fd = fd;                       // define handler
  /*
    通过_epfd拿到并访问kernel define出来的epoll对象
    判断用户态指明的fd是否已经在epoll监视下
    然后根据具体的event来进行对应处理

    valid opcodes ("op" parameter) to issue to epoll_ctl()

    #define EPOLL_CTL_ADD 1  // add a fd to the interface
    #define EPOLL_CTL_EDL 2  // remove a fd from the interface
    #define EPOLL_CTL_MOD 3  // change fd epoll_event structure

    -- epoll_ctl()
    manipulate an epoll instance "epfd"
    fd is the target of the operation
    event describes which events the caller is interested in and any associated user data
   */
  int ret = ::epoll_ctl(_epfd, op, fd, &event);
  error_if(ret == -1, "epoll_ctl() failed!");
  listening.insert(fd);                         // 加入到监听集合
}
// delete only mask event for fd in epoll
void event_loop::del_ioev(int fd, int mask) {
  ioev_it itr = _io_evs.find(fd);
  if (itr == _io_evs.end()) return;
  int& o_mask = itr->second.mask;
  int ret;
  // remove mask from o_mask
  o_mask = o_mask & (~mask);
  if (o_mask == 0){
    _io_evs.erase(itr);
    ret = ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, nullptr);
    listening.erase(fd);
  } else { // 如果没清零 就变成修改操作了？
    struct epoll_event event;
    event.events = o_mask;
    event.data.fd = fd;
    ret = ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &event);
    error_if(ret == -1, "epoll_ctl EPOLL_CTL_MOD failed!");
  }
}
// delete event for fd in epoll
void event_loop::del_ioev(int fd)
{
  _io_evs.erase(fd);
  listening.erase(fd);
  ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, nullptr);
}

/*************************
operations for timer event 
**************************/
int event_loop::run_at(timer_callback cb, void* args, uint64_t ts)
{
  timer_event te(cb, args, ts);
  return _timer_queue->add_timer(te);
}
/* config one-off timer (CLOCK_REALTIME)
   add it to timer_queue
   the timer will run after "secs"
 */
int event_loop::run_after(timer_callback cb, void* args, int sec, int millis)
{
  struct timespec tpc;
  clock_gettime(CLOCK_REALTIME, &tpc);                          // 配置时钟类型
  uint64_t ts = tpc.tv_sec*1000 + tpc.tv_nsec / 1000000UL;      // 加UL做zero extension
  ts += sec*1000 + millis;
  timer_event te(cb, args, ts);
  return _timer_queue->add_timer(te);
}
/* config reuseable timer
   timer will first run after sec+interval
   and will periodically recalled after interval
*/
int  event_loop::run_every(timer_callback cb, void* args, int sec, int millis)
{
  uint32_t interval = sec*1000 + millis;
  struct timespec tpc;
  clock_gettime(CLOCK_REALTIME, &tpc);
  uint64_t ts = tpc.tv_sec*1000 + tpc.tv_nsec / 1000000UL + interval;
  timer_event te(cb, args, ts, interval);
  return _timer_queue->add_timer(te);
}
/* delete timer from _timer_queue
*/
void event_loop::del_timer(int timer_id)
{
  _timer_queue->del_timer(timer_id);
}

/*********************************run event loop********************************/
/* add tasks to _pendingFactors
 */
void event_loop::add_task(pendingFunc func, void* args)
{
  std::pair<pendingFunc, void*> item(func, args);
  _pendingFactors.push_back(item);
}

/* run all the tasks in _pendingFactors and then delete
 */
void event_loop::run_task()
{
  std::vector<std::pair<pendingFunc, void*>>::iterator itr;
  for (itr = _pendingFactors.begin(); itr != _pendingFactors.end(); ++itr)
  {
    pendingFunc func = itr->first;
    void* args = itr->second;
    func(this, args);
  }
  _pendingFactors.clear();
}







