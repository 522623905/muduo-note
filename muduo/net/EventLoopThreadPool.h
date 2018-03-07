// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Types.h>

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

//muduo的TcpServer类通过该类创建多个线程，且每个线程上都运行这一个loop循环 
class EventLoopThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; } //设置开启loop循环的线程数量
  void start(const ThreadInitCallback& cb = ThreadInitCallback());  //启动各个loop线程

  // valid after calling start()
  /// round-robin
  EventLoop* getNextLoop(); //获得loops_中的下一个EventLoop地址

  /// with the same hash code, it will always return the same EventLoop
  EventLoop* getLoopForHash(size_t hashCode);

  std::vector<EventLoop*> getAllLoops();

  bool started() const
  { return started_; }

  const string& name() const
  { return name_; }

 private:

  EventLoop* baseLoop_; // 与Acceptor所属EventLoop相同
  string name_;
  bool started_;
  int numThreads_;  //表示创建多少个loop线程
  int next_;  //新连接到来的loops_的下标
  boost::ptr_vector<EventLoopThread> threads_;  //IO线程列表
  std::vector<EventLoop*> loops_; //EventLoop列表
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
