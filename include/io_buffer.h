#ifndef __IO_BUFFER_H__
#define __IO_BUFFER_H__

#include <list>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <ext/hash_map>


/*
 * 常规设计思路分成三部分
   1. 已经读取的区域
   2. 等待被读取的区域
   3. 空闲空间
 * |-----|-----|----|
 *       |-> head
 *             
 */
class io_buffer
{
// data mmber
public:
  int capacity;     // 总的可用长度
  int length;       // 记录的是待读取区域的长度, payload
  int head;         // header
  io_buffer* next;
  char* data;       // workload
// methods
  io_buffer(int size) : capacity(size), length(0), head(0), next(NULL) {
    data = new char[size];
    assert(data);
  }

  void clear() {
    length = head = 0;
  }

  void adjust() { // move data to head
    if (head) {
      if (length) {
        // 此处注意算不好长度有重叠的可能
        //        dst,  src,       len
        ::memmove(data, data+head, length);
      }
      head = 0;
    }
  }

  /*
   * copy from another io_buffer
   */
  void copy(const io_buffer* other) {
    //        dst  src               
    ::memcpy(data, other->data + other->head, other->length);
    head = 0;
    length = other->length;
  }

  void pop(int len) {
    length -= len;
    head   += len;
  }
};

// wait for implementation
class buffer_pool
{
public:
// public data member
  enum MEM_CAP
  {
    u4K   = 4096,
    u16K  = 16384,
    u64K  = 65536,
    u256K = 262144,
    u1M   = 1048576,
    u4M   = 4194304,
    u8M   = 8388608
  };
// public methods
  static void init(){
    _ins = new buffer_pool;
    assert(_ins);
  }

  static buffer_pool* ins() {
    pthread_once(&_once, init);
    return _ins;
  }

  io_buffer* alloc(int N);                 //主要目的是取回闲置区块
  io_buffer* alloc() {return alloc(u4K);}
  void revert(io_buffer* buffer);          //将闲置区块返回给内存池
private:
  // 3 rule
  buffer_pool();
  buffer_pool(const buffer_pool&);
  const buffer_pool& operator=(const buffer_pool&);

  // define buffer pool structure 实际上是一个hash table
  typedef __gnu_cxx::hash_map<int, io_buffer*> pool_t;
  pool_t _pool;                                        // 自由链表

  uint64_t _total_mem;           // 以GB做单位
  static buffer_pool* _ins;      // 线程安全的指针
  static pthread_mutex_t _mutex;
  static pthread_once_t  _once;
};

struct tcp_buffer
{
// protected data member
protected:
  io_buffer* _buf;           // 当前的区块
public:
// public methods
  tcp_buffer(): _buf(NULL){}
  ~tcp_buffer() {clear();}

  const int length() const {return _buf ? _buf->length : 0;}
  void pop(int len);
  void clear();
};

class input_buffer: public tcp_buffer
{
public:
  int read_data(int fd);
  const char* data() const { return _buf? _buf->data + _buf->head: NULL; }
  void adjust() {if(_buf)_buf->adjust();}
};

class output_buffer: public tcp_buffer
{
public:
  int send_data(const char* data, int datlen);
  int write_fd(int fd);
};

#endif