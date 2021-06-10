#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

class event_loop;

struct commu_head
{
  int cmdid;
  int length;
};

// for acceptor communication with connections for give task to sub-thread
struct queue_msg
{
  enum MSG_TYPE                              // message type
  {
    NEW_CONN,
    STOP_THD,
    NEW_TASK,
  };
  MSG_TYPE cmd_type;                         // manage state
  union {
    int conndf;                              // for new_conn向sub-thread发送新链接
    struct
    {
      void (*task)(event_loop* , void*);
      void* args;
    };                                       // for new_task向下发送待执行任
  };
};

#define COMMU_HEAD_LENGTH 8
#define MSG_LENGTH_LIMIT ((65536 - COMMU_HEAD_LENGTH))

#endif