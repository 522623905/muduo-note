// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& nameArg)
  : mutex_(),
    notEmpty_(mutex_),
    notFull_(mutex_),
    name_(nameArg),
    maxQueueSize_(0),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop(); //停止线程池
  }
}

//创建numThreads个线程并启动
void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);//预留numThreads个空间
  //创建numThreads个数的线程，并启动
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i+1);//线程名
    threads_.push_back(new muduo::Thread(
          boost::bind(&ThreadPool::runInThread, this), name_+id));//创建线程并绑定函数和指定线程名
    threads_[i].start();//启动线程
  }
  //如果线程池为空，且有回调函数，则调用回调函数。这时相当与只有一个主线程
  if (numThreads == 0 && threadInitCallback_)
  {
    threadInitCallback_();
  }
}

//关闭线程池
void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;//停止循环
  notEmpty_.notifyAll();//通知所有等待在任务队列上的线程
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));//每个线程都调用join，等待所有线程退出
}

size_t ThreadPool::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return queue_.size();
}

//往线程池当中的队列添加任务
void ThreadPool::run(const Task& task)
{
  if (threads_.empty())//如果发现线程池中的线程为空，则直接执行任务
  {
    task();
  }
  else //若池中有线程，则把任务添加到队列，并通知线程
  {
    MutexLockGuard lock(mutex_);
    while (isFull())//当任务队列已满
    {
      notFull_.wait();//等待非满通知,再往下执行添加任务到队列
    }
    assert(!isFull());

    //任务队列未满，则添加任务到队列
    queue_.push_back(task);
    notEmpty_.notify();//告知任务队列已经非空，可以执行了
  }
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
void ThreadPool::run(Task&& task)
{    
  if (threads_.empty())
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull())
    {
      notFull_.wait();
    }
    assert(!isFull());

    queue_.push_back(std::move(task));
    notEmpty_.notify();
  }
}
#endif

//从队列获取任务
ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);//任务队列需要保护
  // always use a while-loop, due to spurious wakeup
  //线程已经启动但是队列为空（无任务），则等待,直到线程不为空或running_=false
  while (queue_.empty() && running_)
  {
    notEmpty_.wait();//等待队列不为空，即有任务
  }
  Task task;
  if (!queue_.empty())
  {
    task = queue_.front();//取出队列头的任务
    queue_.pop_front();//并从队列删除该任务
    if (maxQueueSize_ > 0)
    {
      notFull_.notify();//通知，告知任务队列已经非满了，可以放任务进来了
    }
  }
  return task;
}

//判断任务队列是否已满
bool ThreadPool::isFull() const
{
  mutex_.assertLocked();
  return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

//线程要执行的函数（在start()函数中被绑定）
void ThreadPool::runInThread()
{
  try
  {
    if (threadInitCallback_)//如果有回调函数，先调用回调函数。为任务执行做准备
    {
      threadInitCallback_();
    }
    while (running_)
    {
      Task task(take());//取出任务。有可能阻塞在这里，因为任务队列为空
      if (task)
      {
        task();//执行任务
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

