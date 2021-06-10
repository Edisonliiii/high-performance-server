#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "msg_head.h"
#include "thread_queue.h"
#include <pthread.h>

extern void* thread_domain(void* args);

class thread_pool
{
private:
  int _curr_index;
  int _thread_cnt;
  thread_queue<queue_msg>** _pool;
  pthread_t* _tids;
public:
  thread_pool(int thread_cnt);
  thread_queue<queue_msg>* get_next_thread();

  void run_task(int thd_index, pendingFunc task, void* args = nullptr);
  void run_task(pendingFunc task, void* args = nullptr);
};  // thread_pool

#endif