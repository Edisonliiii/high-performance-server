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
  std::queue<queue_msg> msgs; // msg队列
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
      // coming.....
    }
  }
}

void* thread_domain(void* args)
{
  thread_queue<queue_msg>* queue = (thread_queue<queue_msg>*) args;
  event_loop* loop = new event_loop();
  exit_if(loop == nullptr, "new event_loop()");

  queue->set_loop(loop, msg_coming_cb, queue);
  loop->process_evs();
  return nullptr;
}

thread_pool::thread_pool(int thread_cnt): _curr_index(0), _thread_cnt(thread_cnt)
{
  exit_if(thread_cnt <= 0 || thread_cnt > 30, "error thread_cnt %d", thread_cnt);
  _pool = new thread_queue<queue_msg>*[thread_cnt]; // 一个指针数组 核心内容是参量
  _tids = new pthread_t[thread_cnt];
  int ret;
  for (int i=0; i<thread_cnt; ++i)
  {
    _pool[i] = new thread_queue<queue_msg>();
    ret = ::pthread_create(&_tids[i], nullptr, thread_domain, _pool[i]);
    exit_if(ret == -1, "pthread_create");

    ret = ::pthread_detach(_tids[i]);
    error_if(ret == -1, "pthread_detach");
  }
}

thread_queue<queue_msg>* thread_pool::get_next_thread()
{
  if (_curr_index == thread_cnt) _curr_index = 0;
  return _pool[_curr_index++];
}

void thread_pool::run_task(int thd_index, pendingFunc task, void* args = nullptr)
{
  queue_msg msg;
  msg.cmd_type = queue_msg::NEW_TASK;
  msg.task = task;
  msg.args = args;
  thread_queue<queue_msg>* cq = _pool(thd_index);
}

void thread_pool::run_task(pendingFunc task, void* args = nullptr)
{
  for (int i=0; i<_thread_cnt; ++i)
  {
    run_task(i, task);
  }
}