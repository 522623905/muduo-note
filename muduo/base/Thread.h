// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <pthread.h>

namespace muduo
{

//线程对象
class Thread : boost::noncopyable
{
 public:
  typedef boost::function<void ()> ThreadFunc;

  explicit Thread(const ThreadFunc&, const string& name = string());
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  explicit Thread(ThreadFunc&&, const string& name = string());
#endif
  ~Thread();

  void start();//启动线程
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return *tid_; }
  const string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); } //静态成员函数操作静态变量

 private:
  void setDefaultName();//设置线程默认名字为Thread+序号(在Thread构造函数中使用)

  bool       started_;//是否已经启动线程
  bool       joined_;//是否调用了pthread_join()函数
  pthread_t  pthreadId_;//线程ID，（与其他进程中的线程ID可能相同）
  boost::shared_ptr<pid_t> tid_;//线程的真实pid，唯一
  ThreadFunc func_; //线程要回调的函数
  string     name_; //线程名称(若不指定，默认为：Thread+序号)

  static AtomicInt32 numCreated_; //原子性的静态成员。已经创建的线程个数,和其他线程个数有关，因此要static
};

}
#endif
