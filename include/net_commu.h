#ifndef __NET_COMMU_H__
#define __NET_COMMU_H__

#include <stdint.h>

/*
   总体抽象
*/

class net_commu
{
public:
  void* parameter;               // tcp连接内部变量
  net_commu(): parameter(NULL){}
  virtual int send_data(const char* data, int datalen, int cmdid) = 0;
  virtual int get_fd() = 0;
};

#endif