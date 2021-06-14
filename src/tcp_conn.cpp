#include "tcp_conn.h"
#include "msg_head.h"
#include "tcp_server.h"
#include "print_error.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/tcp.h>

static void tcp_rcb(event_loop* loop, int fd, void* args)
{
  tcp_conn* conn = (tcp_conn*)args;
  conn->handle_read();
}

static void tcp_wcb(event_loop* loop, int fd, void* args)
{
  tcp_conn* conn = (tcp_conn*)args;
  conn->handle_write();
}

void tcp_conn::init(int connfd, event_loop* loop)
{
  _connfd = connfd;
  _loop = loop;

  // non-blocking
  int flag = ::fcntl(_connfd, F_GETFL, 0);
  ::fcntl(_connfd, F_SETFL, O_NONBLCK | flag);

  // non-delay
  int opend = 1;
  int ret = ::setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &opend, sizeof(opend));
  error_if(ret<0, "setsockopt TCP_NODELAY");

  if(tcp_server::connBuildCb) tcp_server::connBuildCb(this);

  _loop->add_ioev(_connfd, tcp_rcb, EPOLLIN, this);
  tcp_server::inc_conn();
}

void tcp_conn::handle_read()
{
  int ret = ibuf.read_data(_connfd);
  if (ret == -1)
  {
    // read error
    error_log("read data from socket");
    clean_conn();
    return;
  }
  else if (ret == 0)
  {
    info_log("connection closed by peer");
    clean_conn();
    return;
  }
  commu_head head;
  while(ibuf.length() >= COMMU_HEAD_LENGTH)
  {
    ::memcpy(&head, ibuf.data(), COMMU_HEAD_LENGTH);
    if(head.length > MSG_LENGTH_LIMIT || head.length < 0)
    {
      error_log("data format error in data head, close connection");
      clean_conn();
      break;
    }

    if(ibuf.length() < COMMU_HEAD_LENGTH + head.length)
    {
      // this is half-package
      break;
    }

    // find a dispatcher
    if(!tcp_server::dispatcher.exist(head.cmdid))
    {
      error_log("this message has no corresponding callback, close connection");
      clean_conn();
      break;
    }
    ibuf.pop(COMMU_HEAD_LENGTH);
  }
}

void tcp_conn::handle_write();

void tcp_conn::clean_conn()
{
  // call user defined connCloseCb, clean up user defined resources
  if (tcp_server::connCloseCb) tcp_server::connCloseCb(this);
  // clean up connections
  tcp_server::dec_conn();
  _loop->del_ioev(_connfd);
  _loop = nullptr;
  ibuf.clear();
  obuf.clear();
  int fd = _connfd;
  _connfd = -1;
  ::close(fd);
}












