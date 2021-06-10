#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include <stdint.h>
#include <stdio.h>//NULL

class event_loop; // src from event loop

typedef void io_callback(event_loop* loop, int fd, void* args);     // IO cb
typedef void timer_callback(event_loop* loop, void* usr_data);      // timer cb

typedef void (*pendingFunc)(event_loop*, void*);                    // func ptr

/* 设计出基础的事件结构
 */
// io_event可以改进成为union
struct io_event
{
// data members
  int mask;                // EPOLLIN EPOLLOUT
  io_callback* read_cb;    // cb when EPOLLIN coming
  io_callback* write_cb;   // cb when EPOLLOUT comming
  void* rcb_args;          // extra args for read_cb
  void* wcb_args;          // extra args for write_cb 
//method
  io_event(): read_cb(NULL), write_cb(NULL), rcb_args(NULL), wcb_args(NULL) {}
};

struct timer_event
{
// data
  timer_callback* cb;     // call back function pointer
  void* cb_data;          
  uint64_t ts;            // time slot in seconds 就是时间长度本身
  uint32_t interval;      // interval millsecs 隔多久重新唤醒 0的话就是one-off
  int timer_id;           // 写死，不变
// member
  timer_event(){};          // empty c'tor
  timer_event(timer_callback* timo_cb, void* data, uint64_t arg_ts, uint32_t arg_int=0):
  cb(timo_cb), cb_data(data), ts(arg_ts), interval(arg_int){}
};

#endif