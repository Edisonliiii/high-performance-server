#ifndef __TCP_CONN_H__
#define __TCP_CONN_H__

#include "event_loop.h"
#include "io_buffer.h"
#include "net_commu.h"

class tcp_conn : public net_commu
{
private:
  int _connfd;
  event_loop* _loop;
  input_buffer ibuf;
  output_buffer obuf;
public:
  tcp_conn(int conndf, event_loop* loop) { init(connfd, loop); }

  void init(int connfd, event_loop* loop);
  void handle_read();
  void handle_write();
  void clean_conn();

  virtual int send_data(const char* data, int datlen, int cmdid);
  virtual int get_fd() { return _connfd; }

}; // tcp_conn

#endif