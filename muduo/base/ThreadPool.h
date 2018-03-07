// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace muduo
{
//简单个固定大小线程池，不能自动伸缩
class ThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void ()> Task;

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));//构造函数，默认线程池名字ThreadPool
  ~ThreadPool();

  // Must be called before start().
  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
  void setThreadInitCallback(const Task& cb)
  { threadInitCallback_ = cb; }

  void start(int numThreads);//启动固定的线程数目的线程池
  void stop();//关闭线程池

  const string& name() const
  { return name_; }

  size_t queueSize() const;

  // Could block if maxQueueSize > 0
  void run(const Task& f);//往线程池当中的队列添加任务
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void run(Task&& f);
#endif

 private:
  bool isFull() const;
  void runInThread();//线程池当中的线程要执行的函数
  Task take();//获取任务

  mutable MutexLock mutex_; //互斥量
  //两个条件变量与互斥量一起使用，用于唤醒线程执行任务
  Condition notEmpty_;//非空条件变量
  Condition notFull_;//未满条件变量
  string name_;//线程池名称
  Task threadInitCallback_;//任务
  boost::ptr_vector<muduo::Thread> threads_;//线程池中的线程
  std::deque<Task> queue_;//任务队列
  size_t maxQueueSize_;//最大队列大小，若达到最大队列，则需要等待线程（消费者）取出队列
  bool running_;//线程池是否处于运行状态
};

}

#endif
