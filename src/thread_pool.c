#include "thread_pool.h"
#include "tcp_conn.h"
#include "msg_head.h"
#include "event_loop.h"
#include "print_error.h"
#include "tcp_server.h"

/*
  Parameters:
  - loop: 添加msg到对应的event_loop当中
*/
void msg_coming_cb(event_loop* loop, int fd, void* args)
{
  thread_queue<queue_msg>* queue = (thread_queue<queue_msg>*) args;
  std::queue<queue_msg> msgs; // msg队列, 是当前函数的直接操作对象, 说明queue把内部的queue的值swap给了msgs 自己变成了空 然后直接操作msgs
  queue->recv_msg(msgs);
  while(!msgs.empty())
  {
    queue_msg msg = msgs.front();
    msgs.pop();
    // new connection
    if (msg.cmd_type == queue_msg::NEW_CONN)
    {
      tcp_conn* conn = tcp_server::conns[msg.connfd];
      if (conn)
      {
        conn->init(msg.connfd, loop);
      }
      else
      {
        tcp_server::conns[msg.connfd] = new tcp_conn(msg.connfd. loop);
        exit_if(tcp_server::conns[msg.connfd] == nullptr, "new tcp_conn");
      }
    }
    else if (msg.cmd_type == queue_msg::NEW_TASK)
    {
      loop->add_task(msg.task, msg.args);
    }
    else
    {
      // other msg between threads
    }
  }
}

// the new created thread starts execution by invoking start_routine()
void* thread_domain(void* args)
{
  thread_queue<queue_msg>* queue = (thread_queue<queue_msg>*) args;
  event_loop* loop = new event_loop();
  exit_if(loop == nullptr, "new event_loop()");

  queue->set_loop(loop, msg_coming_cb, queue);
  loop->process_evs();
  return nullptr;
}

/* 初始化的时候thread_pool通过thread_domain+thread_queue充分做到隔离开cb的不同
   初始化的时候就直接开跑每个thread_queue自己的event_loop
   所有thread_queue的运行模式都是一样的
*/
thread_pool::thread_pool(int thread_cnt): _curr_index(0), _thread_cnt(thread_cnt)
{
  exit_if(thread_cnt <= 0 || thread_cnt > 30, "error thread_cnt %d", thread_cnt);
  _pool = new thread_queue<queue_msg>*[thread_cnt]; // 一个指针数组 核心内容是参量, args
  _tids = new pthread_t[thread_cnt]; // methods
  int ret;
  for (int i=0; i<thread_cnt; ++i) // init pool one-by-one
  {
    _pool[i] = new thread_queue<queue_msg>();
    ret = ::pthread_create(&_tids[i], nullptr, thread_domain, _pool[i]);
    exit_if(ret == -1, "pthread_create");
    ret = ::pthread_detach(_tids[i]); // detach the created and configed thread
    error_if(ret == -1, "pthread_detach");
  }
}

thread_queue<queue_msg>* thread_pool::get_next_thread()
{
  // round-robin
  if (_curr_index == thread_cnt) _curr_index = 0;
  return _pool[_curr_index++];
}

// encapsulate the task(args) as queue_msg and send it to its own event_loop
void thread_pool::run_task(int thd_index, pendingFunc task, void* args = nullptr)
{
  queue_msg msg;
  msg.cmd_type = queue_msg::NEW_TASK;
  msg.task = task;
  msg.args = args;
  thread_queue<queue_msg>* cq = _pool[thd_index];
  cq->send_msg(msg);
}

void thread_pool::run_task(pendingFunc task, void* args = nullptr)
{
  for (int i=0; i<_thread_cnt; ++i)
  {
    run_task(i, task); // 本质上是指挥thread_queue不断将msg发送给自己的event_loop
  }
}