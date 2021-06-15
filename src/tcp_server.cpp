#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "msg_head.h"
#include "tcp_conn.h"
#include "tcp_server.h"
#include "print_error.h"
#include "config_reader.h"

void acceptor_cb(event_loop* loop, int fd, void* args)
{
  tcp_server* server = (tcp_server*)args;
  server->do_accept();
}

tcp_server::conn_callback tcp_server::connBuildCb = nullptr;
tcp_server::conn_callback tcp_server::connCloseCb = nullptr;

/*
  [Description]
    c'tor
    one pool per server
    one event loop per server
    1. Initialize tcp server, config listening socket
    2. Config multithreading mode (create thread_pool, tcp_conn pool)
    3. Pass the current tcp server to event loop directly
  [Parameters]
  - ip: numbers-and-dots formation
  - port: running on specific port
  - loop: event loop, one per server
*/
tcp_server::tcp_server(event_loop* loop, const char* ip, uint16_t port)
{
  // clear connaddr
  ::bzero(&_connaddr, sizeof(_connaddr));
  // ignore SIGHUP & SIGPIPE
  // [Todo]: change into sigaction(2) instead
  // sig_ign: ignore the signal
  if(::signal(SIGHUP, SIG_IGN) == SIG_ERR)
  {
    error_log("signal ignore SIGHUP fail");
  }
  if(::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
  {
    error_log("signal ignore SIGPIPE");
  }

  // create socket
  _sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  exit_if(_sockfd == -1, "socket()");
  // ?
  _reservefd = ::open("/tmp/reactor_acceptor", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
  error_if(_reservefd == -1, "open()");

  struct sockaddr_in servaddr;
  ::bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  int ret = ::inet_aton(ip, &servaddr.sin_addr); // dot-and-number to binary
  exit_if(ret==0, "ip format %s", ip);
  servaddr.sin_port = htons(port); // from host byte order to network byte order

  int opend = 1;
  ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opend, sizeof(opend));
  error_if(ret<0, "setsockopt SO_REUSEADDR");

  ret = ::bind(_sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr));
  exit_if(ret==-1, "bind()");

  ret = ::listen(_sockfd, 500);
  exit_if(ret == -1, "listen()");

  info_log("serve on %s:%u is running...", ip, port);


  _loop = loop;
  _addrlen = sizeof(struct sockaddr_in);
  
  // multi-threading reactor, create thread pool
  int thread_cnt = config_reader::ins() -> GetNumber("reactor", "threadNum", 0);
  _thd_pool = nullptr;
  if (thread_cnt) // if there is more than one thread running
  {
    _thd_pool = new thread_pool(thread_cnt);
    exit_if(_thd_pool == nullptr, "new thread_pool");
  }

  // create connection pool
  _max_conns = config_reader::ins()->GetNumber("reactor", "maxConns", 10000);
  int next_fd = ::dup(1);
  _conns_size = _max_conns + next_fd;
  ::close(next_fd);

  conns = new tcp_conn*[_conns_size];
  exit_if(conns==nullptr, "new conns[%d]", _conns_size);
  for(int i=0; i<_max_conns + next_fd; ++i) conns[i]=nullptr; // clear the whole connection pool

  // add acceptor event, pass the current tcp server object to event loop as args
  _loop->add_ioev(_sockfd, acceptor_cb, EPOLLIN, this);
}


tcp_server::~tcp_server()
{
  _loop->del_ioev(_sockfd); // remove current tcp_server from event loop listened by epoll
  ::close(_sockfd); // close fd for the current tcp_server
  ::close(_reservefd); // close reservefd for the current tcp_server
}

void tcp_server::do_accept()
{
  int connfd;
  bool conn_full = false;

  while(true) // accept loop
  {
    // connected socket fd
    connfd = ::accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
    // here errno automatically record the error code from last called function
    if (connfd == -1) // open fd fail!
    {
      // error handler for EINTR, EMFILE, EAGAIN and etc ...
      if(errno == EINTR) // interrupted function fail
      {
        // simply restart
        continue;
      }
      else if(errno == EMFILE) // too many openfiles for this process
      {
        conn_full = true;      // claim conn_full=true
        ::close(_reservefd);
      }
      else if(errno == EAGAIN) // resource temporarily unavailable, break out while loop!
      {
        break;
      }
      else // other error
      {
        exit_log("accept()");
      }
    }
    else if(conn_full) // open fd successful, but conn_full == true
    {
      // 这部分逻辑是一旦conn_full==true, then just close the current connected fd immediatly
      ::close(connfd); // close current fd, release resources
      _reservefd = ::open("/tmp/reactor_acceptor", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
      error_if(_reservefd==-1, "open()");
    }
    else // everything is good
    {
      int curr_conns; //connfd and max connections
      get_conn_num(curr_conns);
      if(curr_conns >= _max_conns) // overnumber
      {
        error_log("connection exceeds the maximum connection count %d", _max_conns);
        ::close(connfd);
      }
      else // under control
      {
        assert(connfd < _conns_size);
        if(_keepalive) // config connected socket as keepalive
        {
          int opend = 1;
          int ret = ::setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &opend, sizeof(opend));
          error_if(ret<0, "setsockopt SO_KEEPALIVE");
        }
        // 通信之间采用的是将connfd封装成queue_msg的方式
        if (_thd_pool) // multi-threading reactor: round-robin event loop
        {
          thread_queue<queue_msg>* cq = _thd_pool->get_next_thread(); //round-robin
          queue_msg msg;
          msg.cmd_type = queue_msg::NEW_CONN;
          msg.connfd = connfd; // pass connected socket
          cq->send_msg(msg);
        }
        else // single thread mode
        {
          tcp_conn* conn = conns[connfd];
          if (conn)
          {
            conn->init(connfd, _loop);
          }
          else
          {
            conn = new tcp_conn(connfd, _loop);
            exit_if(conn == nullptr, "new tcp_conn");
            conns[connfd] = conn;
          }
        }
      }
    }
  }
}


void tcp_server::inc_conn()
{
  ::pthread_mutex_lock(& _mutex);
  ++_curr_conns;
  ::pthread_mutex_unlock(& _mutex);
}

// value-return on cnt
void tcp_server::get_conn_num(int& cnt)
{
  ::pthread_mutex_lock(& _mutex);
  cnt = _curr_conns;
  ::pthread_mutex_unlock(& _mutex);
}

void tcp_server::dec_conn()
{
  ::pthread_mutex_lock(& _mutex);
  --_curr_conns;
  ::pthread_mutex_unlock(& _mutex);
}


























