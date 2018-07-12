// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this), name), //创建线程对象，指定线程函数
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();  //退出loop循环,即退出IO线程
    thread_.join(); //等待线程退出
  }
}

//启动IO线程,并返回EventLoop
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();  //IO线程启动，调用threadFunc()中会执行EventLoop.loop()
  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      cond_.wait(); //等待threadFunc()创建好当前IO线程,notify()时则会返回
    }
  }

  return loop_;
}

//由另一个线程在thread_启动后调用的函数
void EventLoopThread::threadFunc()
{
  EventLoop loop; //创建EventLoop对象。注意，在栈上

  if (callback_)
  {
    callback_(&loop);
  }

// loop_指针指向了一个栈上的对象，threadFunc函数退出之后，这个指针就失效了  
// 就意味着线程退出了，EventLoopThread对象也就没有存在的价值了
// 因而不会有什么大的问题  
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify(); //创建好后通知startLoop()中被阻塞的线程
  }

  loop.loop();  //会在这里循环，直到EventLoopThread析构。此后不再使用loop_访问EventLoop了
  //assert(exiting_);
  loop_ = NULL; //loop_已没用，重新置NULL
}

