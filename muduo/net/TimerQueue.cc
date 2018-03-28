// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()   //创建非阻塞timerfd
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)  //现在距离超时时间when还有多久
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);    //s
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000); //ns
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)  //处理超时事件。超时后，timerfd变为可读
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);  //读timerfd，howmany为超时次数
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

//重新设置timerfd表示的定时器超时时间,并启动
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration);
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);  //设置定时器并开始计时
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),  //创建非阻塞timerfd
    timerfdChannel_(loop, timerfd_),  //该channel负责timerfd分发事件
    timers_(),  //初始定时器集合空
    callingExpiredTimers_(false)  //
{
  timerfdChannel_.setReadCallback(      //设置定时器超时返回可读事件时要执行的回调函数，读timerfd
      boost::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();  //timerfd对应的channel监听事件为可读事件
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second;    //释放Timer*
  }
}

//添加新的定时器
TimerId TimerQueue::addTimer(const TimerCallback& cb,
                             Timestamp when,
                             double interval)
{
  Timer* timer = new Timer(cb, when, interval); // 首先创建一个Timer对象，然后将cb放在里面。内部有一个run函数，调用的就是cb
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer)); // 然后将这个timer丢到eventLoop里面去执行
  return TimerId(timer, timer->sequence());
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
TimerId TimerQueue::addTimer(TimerCallback&& cb,
                             Timestamp when,
                             double interval)
{
  Timer* timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}
#endif

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer); //插入一个定时器，有可能会使得最早到期的定时器发生改变（插入的时间更小时）

  if (earliestChanged)  //要插入的timer是最早超时的定时器
  {
    resetTimerfd(timerfd_, timer->expiration());    //重置定时器的超时时刻
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size()); //相等的。这两个容器保存的是相同的数据，timers_是按到期时间排序，activeTimers_按对象地址排序
  ActiveTimer timer(timerId.timer_, timerId.sequence_); //获取TimerId的Timer*和其序列号给timer
  ActiveTimerSet::iterator it = activeTimers_.find(timer);  //寻找要取消的timer是否在activeTimers_中
  if (it != activeTimers_.end())    //要取消的在当前激活的Timer集合中
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));    //在timers_中取消
    assert(n == 1); (void)n;                                  // Entry:    typedef std::pair<Timestamp, Timer*> Entry;
    delete it->first; // FIXME: no delete please               //timers_:   std::set<Entry>
    activeTimers_.erase(it);    //在activeTimers_中取消
  }
  else if (callingExpiredTimers_)   //如果正在执行超时定时器的回调函数，则加入到cancelingTimers集合中
  {
    cancelingTimers_.insert(timer);//在reset函数中不会再被重启
  }
  assert(timers_.size() == activeTimers_.size());
}

//处理timerfd读事件
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);   //必须要读取timerfd，否则会一直返回就绪事件

  std::vector<Entry> expired = getExpired(now); //找到now之前所有超时的定时器列表

  callingExpiredTimers_ = true;   //设置正在处理超时事件的标志位
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    it->second->run();    //对所有超时的定时器expired执行对应的超时回调函数
  }
  callingExpiredTimers_ = false;    //超时回调结束，清空标志位

  reset(expired, now);    //把要重复运行的定时器重新加入到定时器集合中
}

//找到now之前的所有超时的定时器列表
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;   //用于存放超时的定时器集合
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  TimerList::iterator end = timers_.lower_bound(sentry);  //返回第一个大于等于now的迭代器，小于now的都已经超时
  assert(end == timers_.end() || now < end->first);
  std::copy(timers_.begin(), end, back_inserter(expired));  //[begin end)之间的元素，即达到超时的定时器追加到expired末尾
  timers_.erase(timers_.begin(), end);  //timers_删除小于now的超时定时器

  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());  
    size_t n = activeTimers_.erase(timer);  //activeTimers_删除超时定时器
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired; //返回达到超时的定时器列表
}

//把要重复设置的定时器重新加入到定时器中
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    if (it->second->repeat()    //设置了重复定时且不在cancelingTimers_(要取消的定时器)集合中
        && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it->second->restart(now);   //重启定时器
      insert(it->second); //重新插入倒timers_和activeTimers
    }
    else
    {
      // FIXME move to a free list
        //一次性定时器，不能被重置的，直接删除
      delete it->second; // FIXME: no delete please
    }
  }

  if (!timers_.empty())
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}

//插入一个timer,并返回最早到期时间是否改变
bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  bool earliestChanged = false;//最早到期时间是否改变
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin();
  if (it == timers_.end() || when < it->first)  //比较当前要插入的定时器是否时最早到时的
  {
    earliestChanged = true;
  }
  {
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));   //插入定时器到timers集合中
    assert(result.second); (void)result;
  }
  {
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));  //插入activeTimers_中
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;//返回最早到期时间是否改变bool
}

