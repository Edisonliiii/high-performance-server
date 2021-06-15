#ifndef __THREAD_QUEUE_H__
#define __THREAD_QUEUE_H__

#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <algorithm>
#include <sys/eventfd.h>
#include "event_loop.h"

// 本质是一个事件队列

template <typename T>
class thread_queue
{
  private:
    int _evfd;
    event_loop* _loop;
    std::queue<T> _queue; // thread_queue per se
    pthread_mutex_t _mutex;

  public:
    // c'tor
    thread_queue(): _loop(nullptr)
    {
      ::pthread_mutex_init(& _mutex, nullptr);
      _evfd = ::eventfd(0, EFD_NONBLOCK);
      if (_evfd == -1) 
      {
        perror("eventfd(0, EFD_NONBLOCK)");
        ::exit(1);
      }
    }
    // d'tor
    ~thread_queue()
    {
      ::pthread_mutex_destroy(&_mutex);
      ::close(_evfd);
    }

    // 将前面传过来的queue_msg 直接写到evfd当中
    void send_msg(const T& item)
    {
      unsigned long long number = 1; // buf
      ::pthread_mutex_lock(&_mutex);
      _queue.push(item);
      int ret = ::write(_evfd, &number, sizeof(unsigned long long));
      if (ret == -1) perror("eventfd write");
      ::pthread_mutex_unlock(&_mutex);
    }

    // 直接从evfd读取
    void recv_msg(std::queue<T>& tmp_queue)
    {
      unsigned long long number;
      ::pthread_mutex_lock(&_mutex);
      int ret = ::read(_evfd, number, sizeof(unsigned long long));
      if (ret == -1) perror("eventfd read");
      std::swap(tmp_queue, _queue);
      ::pthread_mutex_unlock(&_mutex);
    }

    event_loop* get_loop() { return _loop; }

    // set loop and install msg comming event's callback: proc
    void set_loop(event_loop* loop, io_callback* proc, void* args = nullptr) 
    {
      _loop = loop;
      _loop->add_ioev(_evfd, proc, EPOLLIN, args);
    }
}

#endif













