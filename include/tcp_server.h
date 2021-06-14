#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include "event_loop.h"
#include "thread_pool.h"
#include "net_commu.h"
#include "tcp_conn.h"
#include "msg_dispatcher.h"
#include <netinet/in.h>

class tcp_server
{
private:
  int _sockfd;
  int _reservfd;
  event_loop* _loop;
  thread_pool* _thd_pool;
  struct sockaddr_in _connaddr;
  socklen_t _addrlen;
  bool _keepalive;

  static int _conns_size;
  static int _max_conns;
  static int _curr_conns;
  static pthread_mutex_t _mutex;

public:
  static msg_dispatcher dispatcher;
  static tcp_conn** conns;

  typedef void (*conn_callback)(net_commu* com);
  static conn_callback connBuildCb;              // 链接建立后的cb
  static conn_callback connCloseCb;              // 链接关闭后的cb

  static void onConnBuild(conn_callback cb) { connBuildCb = cb; }
  static void onConnClose(conn_callback cb) { connCloseCb = cb; }
}; // tcp_server

#endif