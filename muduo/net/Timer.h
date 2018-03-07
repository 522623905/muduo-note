// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>

namespace muduo
{
namespace net
{
///
/// Internal class for timer event.
///
//Timer封装了定时器的一些参数，例如超时回调函数、超时时间、定时器是否重复、重复间隔时间、定时器的序列号。
//其函数大都是设置这些参数，run()用来调用回调函数，restart()用来重启定时器（如果设置为重复）。 
class Timer : boost::noncopyable       
{                                                            
 public:
  Timer(const TimerCallback& cb, Timestamp when, double interval)
    : callback_(cb),  //回调函数
      expiration_(when),  //超时时间
      interval_(interval),  //如果重复，间隔时间
      repeat_(interval > 0.0),  //是否重复
      sequence_(s_numCreated_.incrementAndGet())  //设置当前定时器序列号，原子操作,先加后获取
  { }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  Timer(TimerCallback&& cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())
  { }
#endif

  //超时时调用的回调函数
  void run() const
  {
    callback_();
  }

  Timestamp expiration() const  { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  const TimerCallback callback_;    //回调函数
  Timestamp expiration_;    //超时时间（绝对时间）
  const double interval_;   //间隔多久重新闹铃
  const bool repeat_;   //是否重复
  const int64_t sequence_;    //Timer序号

  static AtomicInt64 s_numCreated_;   //Timer计数，当前已经创建的定时器数量
};  
}
}
#endif  // MUDUO_NET_TIMER_H
