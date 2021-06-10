#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include "../include/timer_queue.h"
#include "../include/print_error.h"

/*
  struct timespec{
    time_t tv_sec;     // whole seconds
    long tv_nsec;      // nanoseconds
  };

  struct itimerspec{
    struct timespec it_interval; // timer interval
    struct timespec it_value;    // initial expiration
  };
*/

/************************* private implementation
此处用一个最小堆来管理整个vector中的数据
**************************/
void timer_queue::reset_timo()
{
  struct itimerspec old_ts, new_ts;
  ::bzero(&new_ts, sizeof(new_ts));
  // if _pioneer has been initialized
  /* 这里说明一下 itimerspec里边配置时间是这样的
     例如it_interval有两个数据成员 tv_sec, tv_nsec
     两个的联系是 tv_sec.tv_nsec 组合使用
  */
  if (_pioneer != (uint64_t)-1) {
    new_ts.it_value.tv_sec = _pioneer / 1000;
    new_ts.it_value.tv_nsec = (_pioneer % 1000) * 1000000;
  }
  // when _pioneer == -1, new_ts = 0 will disarms the timer
  // disarms the timer referred to by the fd
  // 通过fd给对应的timer object根据new_ts配置时间
  ::timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &new_ts, &old_ts);
}

void timer_queue::heap_add(timer_event& te)
{
  _event_lst.push_back(te);
  // update position
  _position[te.timer_id] = _count;
  int curr_pos = _count;
  ++_count;

  // parent postion
  int prt_pos = (curr_pos - 1) >> 1;
  // MIN heap update
  while (prt_pos >=0 && _event_lst[curr_pos].ts < _event_lst[prt_pos].ts)
  {
  	// swap
    timer_event tmp = _event_lst[curr_pos];
    _event_lst[curr_pos] = _event_lst[prt_pos];
    _event_lst[prt_pos] = tmp;

    // update position
    _position[_event_lst[curr_pos].timer_id] = curr_pos;
    _position[tmp.timer_id] = prt_pos;

    curr_pos = prt_pos;
    prt_pos = (curr_pos-1) >> 1;
  }
}

void timer_queue::heap_del(int pos)     //_position[pos]
{
  if (_event_lst.size()==0 && _position.size()==0) return;
  _position.erase(_event_lst[pos].timer_id);
  
  // heap del, swithc to the last one
  std::swap(_event_lst[pos], _event_lst[_count-1]);
  _position[_event_lst[pos].timer_id] = pos;

  --_count;
  _event_lst.pop_back();
  if (_event_lst.size()==0) _position.erase(_event_lst[pos].timer_id); 
  // pop down
  heap_hold(pos);
}

void timer_queue::heap_pop(){
  if (_count <= 0) return;
  _position.erase(_event_lst[0].timer_id);
  
  if (_count > 1){
    std::swap(_event_lst[0], _event_lst[_count-1]);
    // update
    _position[_event_lst[0].timer_id] = 0;
    _event_lst.pop_back();
    --_count;
    heap_hold(0); // top-down
  } else if (_count == 1){
    _event_lst.pop_back();
    --_count;
  }
}

// heapify
void timer_queue::heap_hold(int pos)
{
  // top to down ONLY
  // 这个timer_queue的使用场景 1. 在最后一位添加新node heap_add自己会bottom-up往上更新
  //                        2. pop掉第一位 这时候用到top-down更新
  //                        3. 如果需要修改任意一位 就需要另写一个往上更新的heapify
  int l = (pos<<1)+1, r = (pos<<1)+2, tmp = pos;
  while (l <= _count-1)
  {
  	if (r>_count-1) tmp = l;
  	else if(_event_lst[l].ts < _event_lst[r].ts) tmp = l;
  	else tmp = r;

  	if(_event_lst[pos].ts < _event_lst[tmp].ts) break;
    // swap elements, could be swapped
    std::swap(_event_lst[pos],_event_lst[tmp]);
    std::swap(_position[_event_lst[pos].timer_id], _position[_event_lst[tmp].timer_id]);
    // check continuty
    pos = tmp;
    l = (pos<<1)+1;
    r = (pos<<1)+2;
  }
}

/*************************public implementation
**************************/
timer_queue::timer_queue():
_count(0), _next_timer_id(0), _pioneer(-1/*= uint32_t max*/)
{
  // create and operate on a timer that delivers timer expiration notifications via a fd
  // 这里不能直接访问timer本身 能接触到的也只是fd 作为一个timer object的reference
  _timerfd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
  exit_if(_timerfd == -1, "timerfd_create()");
}

timer_queue::timer_queue(timer_queue&& _other)
{
  if (this == &_other) return;
  std::copy(this->_event_lst.begin(), this->_event_lst.end(), _other._event_lst.begin());
  this->_position      = _other._position;
  this->_count         = _other._count;
  this->_next_timer_id = _other._count;
  this->_timerfd       = _other._timerfd;
  this->_pioneer       = _other._pioneer;
}

// assginment copy c'tor (move)
timer_queue& timer_queue::operator=(timer_queue&& _other)
{
  if (this == &_other) return *this;
  this->_event_lst = std::move(_other._event_lst);
  this->_position  = std::move(_other._position);
  this->_count         = _other._count;
  this->_next_timer_id = _other._count;
  this->_timerfd       = _other._timerfd;
  this->_pioneer       = _other._pioneer;

  _other._event_lst.clear();
  _other._position.clear();
  _other._count = 0;
  _other._count = 0;
  _other._timerfd = 0;
  _other._pioneer = 0;
  return *this;
}

timer_queue::~timer_queue()
{
  ::close(_timerfd);
}

int timer_queue::add_timer(timer_event& te)
{
  te.timer_id = _next_timer_id++;
  heap_add(te);
  if(_event_lst[0].ts < _pioneer)
  {
    _pioneer = _event_lst[0].ts;
    reset_timo();
  }
  return te.timer_id;
}

void timer_queue::del_timer(int timer_id)
{
  mit it = _position.find(timer_id);
  if (it == _position.end()){
    error_log("no such a timerid %d", timer_id);
    return;
  }

  int pos = it->second;
  heap_del(pos);
  
  if (_count == 0){
    _pioneer = -1;
    reset_timo();
  }
  else if (_event_lst[0].ts < _pioneer)
  {
    _pioneer = _event_lst[0].ts;
    reset_timo();
  }
}

/* implement non-once timer reuse
   1. get min-heaped timer to fired_evs
   2. if it's reuseable, push back to reuse lst
   3. iterate reuse lst, push back available timer all back to event_lst
      and don't forget update
*/
void timer_queue::get_timo(std::vector<timer_event>& fired_evs)
{
  std::vector<timer_event> _reuse_lst;
  //取出最先到期的timer 放到fired_evs中
  while(_count != 0 && _pioneer == _event_lst[0].ts)
  {
  	timer_event te = _event_lst[0];
  	fired_evs.push_back(te);
  	// 如果不是一次性timer(interval!=0),同时也加入到_reuse_lst中
  	if (te.interval){
  	  te.ts += te.interval;
  	  _reuse_lst.push_back(te);
  	}
  	// 从event_lst中pop掉
  	heap_pop();
    //_pioneer = _event_lst[0].ts;           // ？为什么pioneer写死一次不改变
  }
  // 遍历整个_reuse_lst 把所有队列中的timer都加入到event_lst中
  for (vit it = _reuse_lst.begin(); it != _reuse_lst.end(); ++it)
  {
  	add_timer(*it);
  }
  // reset the timer 因为重新像event_lst中加过项目之后就要update
  if (_count == 0) {
    _pioneer = -1;
    reset_timo();
  } else {
    _pioneer = _event_lst[0].ts;
    reset_timo();
  }
}






























