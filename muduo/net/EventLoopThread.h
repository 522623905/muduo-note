// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

class EventLoop;

/*
任何一个线程，只要创建并运行了EventLoop，就是一个IO线程。
EventLoopThread类就是一个封装了的IO线程
*/

class EventLoopThread : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();
  EventLoop* startLoop(); //启动线程，该线程就成为了IO线程 ，返回本线程中的EventLoop

 private:
  void threadFunc();  //线程函数

  EventLoop* loop_; //本线程持有的EventLoop对象指针
  bool exiting_;  //是否已经退出
  Thread thread_; //本线程
  MutexLock mutex_; //互斥锁
  Condition cond_;  //条件变量
  ThreadInitCallback callback_; // 回调函数在EventLoop::loop事件循环之前被调用
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

