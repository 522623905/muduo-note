// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
//__thread表示线程局部变量，否则则由于线程性质，则是共享的！
//每个线程存储当前线程的EventLoop对象指针。
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;//Poll的超时时间

//创建非阻塞eventfd
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    //忽略SIGPIPE信号，防止对方断开连接时继续写入造成服务进程意外退出
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;//这里定义后即可忽略SIGPIPE信号
}

//返回当前线程的EventLoop对象（one loop per thread）
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;//__thread线程局部变量
}

//不能跨线程调用，只能在创建EventLoop的线程使用！
//one loop per thread
EventLoop::EventLoop()
  : looping_(false),  //表示还未循环
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),      //存储创建该对象的本线程的ID
    poller_(Poller::newDefaultPoller(this)),  //构造了一个实际的poller对象
    timerQueue_(new TimerQueue(this)), //用于管理定时器
    wakeupFd_(createEventfd()),    //创建eventfd，用于唤醒线程用(跨线程激活,使用eventfd线程之间的通知机制)
    wakeupChannel_(new Channel(this, wakeupFd_)), //与wakeupFd_绑定
    currentActiveChannel_(NULL)
{
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread)       //检查当前线程是否创建了EventLoop对象（one loop per thread）
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;  //如果当前线程，已经有了一个EventLoop对象，则LOG_FATAL终止程序。(遵循one loop per thread)
  }
  else
  {
    t_loopInThisThread = this;    //保存当前EventLoop对象指针
  }
  wakeupChannel_->setReadCallback(
      boost::bind(&EventLoop::handleRead, this)); //设置eventfd唤醒线程后的读回调函数
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();//开启eventfd的读事件
}

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

//loop()函数中当poll()返回时，会遍历活跃通道表activeChannels，
//并且执行它们每一个提前注册的handleEvent()函数，处理完了会继续循环
void EventLoop::loop()
{
  assert(!looping_);  //断言不处于事件循环
  assertInLoopThread(); //断言在创建EventLoop对象的线程调用loop，因为loop函数不能跨线程调用
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear(); //首先清除上一次的活跃channel
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); //使用epoll_wait等待事件到来，并把到来的事件填充至activeChannels
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();  //日志登记，日志打印
    }
    // TODO sort channel by priority
    eventHandling_ = true;
    //逐一取出活动的事件列表，并执行相关回调函数
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;//当前正在处理的活动通道
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;  //处理完了赋空
    eventHandling_ = false;

    // 执行pending Functors_中的任务回调
    // 这种设计使得IO线程也能执行一些计算任务，避免了IO线程在不忙时长期阻塞在IO multiplexing调用中
    doPendingFunctors(); 
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

//结束循环.可跨线程调用
void EventLoop::quit()
{
  quit_ = true;
  // There is a chance that loop() just executes while(!quit_) and exits,
  // then EventLoop destructs, then we are accessing an invalid object.
  // Can be fixed using mutex_ in both places.
  if (!isInLoopThread())
  {
    wakeup();  //如果不是IO线程调用，则需要唤醒IO线程。因为此时IO线程可能正在阻塞或者正在处理事件
  }
}

//在它的IO线程内执行某个用户任务回调,避免线程不安全的问题，保证不会被多个线程同时访问。
void EventLoop::runInLoop(const Functor& cb)
{
  if (isInLoopThread()) //若是在当前IO线程，则直接调用回调
  {
    cb();
  }
  else    //不是当前IO线程，则加入队列，等待IO线程被唤醒再调用
  {
    queueInLoop(cb);
  }
}

//将任务放到pendingFunctors_队列中并通过evnetfd唤醒IO线程执行任务
void EventLoop::queueInLoop(const Functor& cb)  
{
  // 把任务加入到队列可能同时被多个线程调用，需要加锁
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(cb); //把回调函数加入到队列当中
  }

  // 将cb放入队列后，我们还需要唤醒IO线程来及时执行Functor
  // 有两种情况：
  // 1.如果调用queueInLoop()的不是IO线程，需要唤醒,才能及时执行doPendingFunctors()
  // 2.如果在IO线程调用queueInLoop()，且此时正在调用pending functor(原因：
  //    防止doPendingFunctors()调用的Functors再次调用queueInLoop，
  //    循环回去到poll的时候需要被唤醒进而继续执行doPendingFunctors()，否则新增的cb可能不能及时被调用),
  // 即只有在IO线程的事件回调中调用queueInLoop()才无需唤醒(即在handleEvent()中调用queueInLoop ()不需要唤醒
  //    ，因为接下来马上就会执行doPendingFunctors())
  if (!isInLoopThread() || callingPendingFunctors_) 
  {
    wakeup(); //写一个字节来唤醒poll阻塞，触发wakeupFd可读事件
  }
}

//返回任务队列pendingFunctors_大小
size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size();
}

//某个时间点执行定时回调
TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

//某个时间点之后执行定时回调
TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

//在每个时间间隔后处理某个回调事件
TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
// FIXME: remove duplication
void EventLoop::runInLoop(Functor&& cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(std::move(cb));
  }
}

void EventLoop::queueInLoop(Functor&& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));  // emplace_back
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Timestamp& time, TimerCallback&& cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}
#endif

//删除timerId对应的定时器
void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

//更新通道,将channel对应的fd事件注册或更改到epoll内核事件表中
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

//从poller中移除通道
void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

//不在IO线程,则退出程序
void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

//写8个字节给eventfd，唤醒事件。
//为的是使IO线程能及时处理Functor,否则EventLoop::loop()的poll会阻塞
void EventLoop::wakeup()
{
  uint64_t one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

 //将eventfd里的内容读走，以便让其继续检测事件通知
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one); //将这个wakeupFd_ 上面数据读出来，不然的话下一次又会被通知到
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

//执行pendingFunctors_中的任务
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true; //设置标志位,表示当前在执行Functors任务

  //好！swap到局部变量中，再下面回调！
  //好处：１.缩减临界区长度,意味这不会阻塞其他线程调用queueInLoop
  //       2.避免死锁(因为Functor可能再调用queueInLoop)
  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_);
  }

  //依次执行functors
  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

