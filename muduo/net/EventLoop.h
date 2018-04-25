// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <vector>

#include <boost/any.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TimerId.h>

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
class EventLoop : boost::noncopyable
{
 public:
  typedef boost::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for scoped_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void loop();  //此接口为该类的核心接口，用来启动事件循环

  /// Quits loop.
  ///
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<EventLoop> for 100% safety.
  void quit();//退出主循环

  ///
  /// Time when poll returns, usually means data arrival.
  ///
  Timestamp pollReturnTime() const { return pollReturnTime_; }//poll延迟的时间

  int64_t iteration() const { return iteration_; }//迭代次数

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(const Functor& cb);  //用来将非io线程内的任务放到pendingFunctors_中并唤醒wakeupChannel_事件来执行任务(在主循环中运行)
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor& cb);  //插入主循环任务队列,用来将非io线程内的任务放到pendingFunctors_中并唤醒wakeupChannel_事件来执行任务

  size_t queueSize() const;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void runInLoop(Functor&& cb);
  void queueInLoop(Functor&& cb);
#endif

  // timers

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId runAt(const Timestamp& time, const TimerCallback& cb);  //某个时间点执行定时回调
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId runAfter(double delay, const TimerCallback& cb);  //某个时间点之后执行定时回调
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId runEvery(double interval, const TimerCallback& cb); //在每个时间间隔处理某个回调事件
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void cancel(TimerId timerId);//删除某个定时器

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  TimerId runAt(const Timestamp& time, TimerCallback&& cb);
  TimerId runAfter(double delay, TimerCallback&& cb);
  TimerId runEvery(double interval, TimerCallback&& cb);
#endif

  // internal usage
  void wakeup();//写8个字节给eventfd，唤醒事件通知描述符。否则EventLoop::loop()的poll会阻塞
  void updateChannel(Channel* channel); //在poller中注册或者更新通道
  void removeChannel(Channel* channel); //从poller中移除通道
  bool hasChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  void assertInLoopThread() //断言处于当前线程中（主要是因为有些接口不能跨线程调用）
  {
    if (!isInLoopThread())
    { 
      abortNotInLoopThread();//如果不是，则终止程序
    }
  }
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }  //判断是是否处于同一线程，而不是跨线程
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool eventHandling() const { return eventHandling_; }//是否正在处理事件

  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();//不在主I/O线程
  void handleRead();   //将事件通知描述符里的内容读走，以便让其继续检测事件通知
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;

  bool looping_; /* atomic */   //是否处于事件循环
  bool quit_; /* atomic and shared between threads, okay on x86, I guess. */ //是否退出loop
  bool eventHandling_; /* atomic */   //当前是否处于事件处理的状态
  bool callingPendingFunctors_; /* atomic */
  int64_t iteration_;
  const pid_t threadId_;    //EventLoop构造函数会记住本对象所属的线程ID
  Timestamp pollReturnTime_;  //poll返回的时间戳
  boost::scoped_ptr<Poller> poller_;  //EventLoop首先一定得有个I/O复用才行,它的所有职责都是建立在I/O复用之上的
  boost::scoped_ptr<TimerQueue> timerQueue_;  //应该支持定时事件，关于定时器的所有操作和组织定义都在类TimerQueue中 
  int wakeupFd_; //用于eventfd的通知机制的文件描述符
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  boost::scoped_ptr<Channel> wakeupChannel_;  //wakeupFd_对于的通道。若此事件发生便会一次执行pendingFunctors_中的可调用对象
  boost::any context_;  //用来存储用户想要保存的信息，boost::any任何类型的数据都可以

  // scratch variables
  ChannelList activeChannels_;  //保存的是poller类中的poll调用返回的所有活跃事件集
  Channel* currentActiveChannel_; //当前正在处理的活动通道

  mutable MutexLock mutex_;
  std::vector<Functor> pendingFunctors_; // @GuardedBy mutex_
};                  //当非io线程(搭载EventLoop的线程)想使某个任务放在io线程中来执行，
                    //那么就可以将其放到数据成员pendingFunctors_中来
                    //其对应的事件便是wakeupChannel_事件
}
}
#endif  // MUDUO_NET_EVENTLOOP_H
