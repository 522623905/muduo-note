// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Channel.h>

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///虽然TimerQueue中有Queue，但是其实现是基于Set的，而不是Queue。
///这样可以高效地插入、删除定时器，且找到当前已经超时的定时器。TimerQueue的public接口只有两个，添加和删除。
class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  ///  添加定时器，线程安全
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  TimerId addTimer(TimerCallback&& cb,
                   Timestamp when,
                   double interval);
#endif

  //取消定时器，线程安全
  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  typedef std::pair<Timestamp, Timer*> Entry;  //std::pair支持比较运算,存储超时时间戳和Timer*指针,保证了相同的 Timestamp，但 Timer地址不同，因此可以处理多个相同时间的定时事件
  typedef std::set<Entry> TimerList;  //元素为超时时间和指向超时的定时器,会按照时间戳来排序(Timestamp重载了<运算符)
  typedef std::pair<Timer*, int64_t> ActiveTimer; //Timer*指针和定时器序列号
  typedef std::set<ActiveTimer> ActiveTimerSet; //元素为定时器和其序列号,按照Timer* 地址大小来排序

  //以下成员函数只可能在其所属IO线程中调用，因而不必加锁
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);

  // called when timerfd alarms
  void handleRead();//处理timerfd读事件，执行超时函数
  // move out all expired timers
  std::vector<Entry> getExpired(Timestamp now); //返回超时的定时器列表,并把超时的定时器从集合中删除
  void reset(const std::vector<Entry>& expired, Timestamp now);//把要重复运行的定时器重新加入到定时器集合中

  bool insert(Timer* timer);// 插入定时器

  EventLoop* loop_;// 所属的EventLoop
  const int timerfd_;//timefd加入epoll,超时可读
  Channel timerfdChannel_;  //用于观察timerfd_的readable事件（超时则可读）
  // Timer list sorted by expiration
  TimerList timers_;    //定时器集合，按到期时间排序

  // for cancel()
  ActiveTimerSet activeTimers_;//定时器集合,按照Timer* 地址大小来排序
  bool callingExpiredTimers_; /* atomic */  //是否正在处理超时定时事件
  ActiveTimerSet cancelingTimers_;  //保存要取消的定时器的集合(如果不在定时器集合中,而是属于在执行超时回调的定时器)
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
