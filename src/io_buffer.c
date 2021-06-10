#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include "io_buffer.h"
#include "print_error.h"


#define EXTRA_MEM_LIMIT (5u*1024*1024) // upto 5GB



/******************** utility ************************/
/*
 * allocate at initialization
 */
static inline volatile 
void _pool_initialization(io_buffer*& prev, int pool_key, int block_num)
{
  _pool[pool_key] = new io_buffer(pool_key);
  exit_if(_pool[pool_key]==NULL, "new io_buffer");
  prev = _pool[pool_key];
  for (int i=1; i<block_num; ++i)
  {
    prev->next = new io_buffer(pool_key);
    exit_if(prev->next==NULL, "new io_buffer");
    prev = prev->next;
  }
  _total_mem += (pool_key/1024)*block_num;
}
/*****************************************************/

/* 
 * buffer_pool implementation 
 */
io_buffer* buffer_pool::alloc(int N)
{
  int index;
  if (N <= u4K) index = u4K;
  else if (N <= u16K)   index = u16K;
  else if (N <= u64K)   index = u64K;
  else if (N <= u256K)  index = u256K;
  else if (N <= u1M)    index = u1M;
  else if (N <= u4M)    index = u4M;
  else if (N <= u8M)    index = u8M;
  else return NULL;

  ::pthread_mutex_lock(&_mutex);
  // 根据N的大小找到hash table中对应的自由链表
  if (!_pool[index]){ // 如果对应链表为空
    // 超界判断
    if (_total_mem + index / 1024 >= EXTRA_MEM_LIMIT) {
      exit_log("too many memory");
      ::exit(1);
    }
    // 否则新开一段内存 而后直接返回
    io_buffer* new_buf = new io_buffer(index);
    exit_if(new_buf == NULL, "new io_buffer");
    
    // 增加totalmem的大小
    _total_mem += index / 1024;
    ::pthread_mutex_unlock(&_mutex);
    return new_buf;
  }
  /* 如果对应链表不是空
     这是一个取的操作 从自由链表抽出第一个闲置区块
   */
  io_buffer* target = _pool[index];
  _pool[index] = target->next;
  ::pthread_mutex_unlock(&_mutex);

  target->next = NULL;
  return target;
}

void buffer_pool::revert(io_buffer* buffer)
{
  int index = buffer->capacity;
  buffer->length = 0;
  buffer->head   = 0;

  ::pthread_mutex_lock(&_mutex);

  assert(_pool.find(index)!=_pool.end());
  buffer->next = _pool[index];
  _pool[index] = buffer

  ::pthread_mutex_unlock(&_mutex);
}

buffer_pool::buffer_pool(): _total_mem(0)
{
  io_buffer* prev; 
  // 4K*5000 ≈ 20MB
  _pool_initialization(prev, u4K, 5000);
  // 16K*1000 ≈ 16MB
  _pool_initialization(prev, u16K, 1000);
  // 64K*500  ≈ 32MB
  _pool_initialization(prev, u64K, 500);
  // 256K*100 ≈ 25MB
  _pool_initialization(prev, u256K, 100);
  // 1M*50 = 50MB
  _pool_initialization(prev, u1M, 50);
  // 4M*20 = 80MB
  _pool_initialization(prev, u4M, 20);
  // 8M*10 = 80MB
  _pool_initialization(prev, u8M, 10);
}

/*
 * initialize some static data members
 */
buffer_pool* buffer_pool::_ins = NULL;
pthread_mutex_t buffer_pool::_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_once_t  buffer_pool::_once  = PTHREAD_ONCE_INIT;


/*
 * tcp_buffer implementation
 */

/* return idle block back to the pool*/
void tcp_buffer::clear()
{
  if (_buf){
    buffer_pool::ins()->revert(_buf);
    _buf = NULL;
  }
}

void tcp_buffer::pop(int len)
{
  assert(_buf && len <= _buf->length);
  _buf->pop(len);
  if (!_buf->length){   // 当前区块长度为0就说明当前区块已完全闲置 返回给内存池
    buffer_pool::ins()->revert(_buf);
    _buf = NULL;
  }
}

/*
 * input_buffer implementation
   需要把read_data & send_data重构一下
 */

/* 模拟系统调用的操作 返回值都是一个道理 */
int input_buffer::read_data(int fd)
{
  // 一次性读所有数据
  int rn, ret;
  // ioctl -- 读入对应硬件缓冲区的字节
  if (::ioctl(fd, FIONREAD, &rn) == -1){
    error_log("ioctl FIONREAD");
    return -1;
  }

  if (!_buf){    // if buf is blank, alloc
    _buf = buffer_pool::ins()->alloc(rn);
    if (!_buf) { // alloc fail!
      error_log("no idle for alloc io_buffer");
      return -1;
    }
  }
  else           // if buf is NOT blank
  {
    assert(_buf->head == 0);
    // if capacity is not sufficient, open a new one
    if (_buf->capacity - _buf->length < rn){
      io_buffer* new_buf = buffer_pool::ins()->alloc(rn + _buf->length);
      if (!new_buf){ // error handler
        error_log("no idle for alloc io_buffer");
        return -1;
      }
      new_buf->copy(_buf);               // 搬家
      buffer_pool::ins()->revert(_buf);  // 原来的还给内存池
      _buf = new_buf;
    }
  }

  // 只要fd有内容就读出来 一次读出来之后就跳出循环
  do
  {
    ret = ::read(fd, _buf->data+_buf->length, rn)
  } while (ret == -1 && errno == EINTR);

  if (ret > 0){
    assert(ret == rn);
    _buf->length += ret;
  }

  return ret;
}

int output_buffer::send_data(const char* data, int datlen)
{
  if (!_buf){
    _buf = buffer_pool::ins()->alloc(datlen);
    if (!_buf){
      error_log("no idle for alloc io_buffer");
      return -1;
    }
  }
  else
  {
    assert(_buf->head==0);
    if (_buf->capacity - _buf->length < datlen){
      io_buffer* new_buf = buffer_pool::ins()->alloc(datlen);
      if (!new_buf){
        error_log("no idle for alloc io_buffer");
        return -1;
      }
      new_buf->copy(_buf);
      buffer_pool::ins()->revert(_buf);
      _buf = new_buf;
    }
  }

  ::memcpy(_buf->data+_buf->length, data, datlen);
  _buf->length += datlen;
  return 0;
}

int output_buffer::write_fd(int fd)
{
  assert(_buf && _buf->head==0);
  int writed;
  do{
    writed = ::write(fd,_buf->data, _buf->length);
  }while(writed==-1 && errno ==EINTR);

  if(writed>0){        // 腾出来写过的位置
    _buf->pop(writed);
    _buf->adjust();
  }
  if(writed==-1 && errno==EAGAIN){
    writed = 0;    // 不能继续写了
  }
  return writed;
}



































