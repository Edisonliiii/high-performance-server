#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "msg_head.h"
#include "tcp_client.h"
#include "print_error.h"

#define BUF_SIZE 4194304


tcp_client(event_loop* loop, const char* ip, 
           unsigned short port, const char* name = NULL):
           net_ok(false),
           ibuf(BUF_SIZE),
           obuf(BUF_SIZE),
           _sockfd(-1),
           _loop(loop),
           _onconnection(NULL),
           _onconn_args(NULL),
           _onclose(NULL),
           _onclose_args(NULL),
           _name(name)
{
// construct server address
  ::bzero((char*)&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((unsigned short)port);
  int hostaddr = ::inet_ntoa(ip, &servaddr.sin_addr);
  exit_if(ret == 0, "ip format %s", ip);
  _addrlen = sizeof(servaddr);

  // connect
  do_connect();
}

void tcp_client::do_connect()
{
  if(_sockfd != -1) ::close(_sockfd);
  // create socket
  _sockfd = ::sock(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_TCP);
  exit_if(_sockfd == -1, "socket()");

  int ret = ::connect(_sockfd, (const struct sockaddr*)&servaddr, _addrlen);
  if (ret == 0)
  {
    net_ok = true;
    call_onconnect();
    info_log("connect %s:%d successfully", ::inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
  } else {
    if (errno == EINPROGRESS)
    {
      _loop->add_ioev(_sockfd, connection_cb, EPOLLOUT, this);
    } else {
       exit_log("connect()");
    }
  }
}

int send_data(const char* data, int datlen, int cmdid)
{
  if (!net_ok) return -1;    // error handler
  bool need = (obuf.length == 0) ? true : false;

}
virtual int get_fd() {return _sockfd};
int handle_read();
int handle_write();
void clean_conn();