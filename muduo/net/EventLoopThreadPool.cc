// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThreadPool.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg)
  : baseLoop_(baseLoop),
    name_(nameArg),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

// 启动线程池
void EventLoopThreadPool::start(const ThreadInitCallback& cb) 
{
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  /* 创建numThreads_个EventLoop io线程*/
  for (int i = 0; i < numThreads_; ++i)
  {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    EventLoopThread* t = new EventLoopThread(cb, buf);
    threads_.push_back(t);//当ptr_vector<EventLoopThread>对象销毁，其所管理的EventLoopThread也跟着销毁
    loops_.push_back(t->startLoop()); // startLoop()会创建并返回新的EventLoop,然后push_back到loops_
  }
  if (numThreads_ == 0 && cb)
  {
    cb(baseLoop_);  // 只有一个EventLoop，在这个EventLoop进入事件循环之前，调用cb
  }
}

//获得loops_中的下一个EventLoop地址
//当一个新的对象到来的时候 选择一个EventLoop来处理
EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop* loop = baseLoop_;/* baseLoop_是Acceptor所属EventLoop，即mainReactor*/

  // 如果loops_为空，则loop指向baseLoop_ 所有任务由mainReactor处理
  // 如果不为空，按照round-robin（RR，轮叫）的调度方式选择一个EventLoop
  if (!loops_.empty())
  {
    loop = loops_[next_];
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
}

//指定某一个loop，强制获取
EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_;


  if (!loops_.empty())
  {
    loop = loops_[hashCode % loops_.size()];  //根据hashCode分配
  }
  return loop;
}

//获取所有的loops列表
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  if (loops_.empty())
  {
    return std::vector<EventLoop*>(1, baseLoop_);
  }
  else
  {
    return loops_;
  }
}
