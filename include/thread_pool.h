#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "msg_head.h"
#include "thread_queue.h"
#include <pthread.h>

extern void* thread_domain(void* args);

class thread_pool
{
private:
  int _curr_index;                      // starts from 0
  int _thread_cnt;                      // #thread [0,30)
  thread_queue<queue_msg>** _pool;      // 线程池当中的thread_queue是链表式管理, thread_pool per se, args
  pthread_t* _tids;                     // multitheads per se, methods
public:
  thread_pool(int thread_cnt);
  thread_queue<queue_msg>* get_next_thread();

  void run_task(int thd_index, pendingFunc task, void* args = nullptr);
  void run_task(pendingFunc task, void* args = nullptr);
};  // thread_pool

#endif