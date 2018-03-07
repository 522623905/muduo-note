// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CURRENTTHREAD_H
#define MUDUO_BASE_CURRENTTHREAD_H

#include <stdint.h>

namespace muduo
{
namespace CurrentThread
{
  // internal
    //__thread修饰的变量是线程局部存储的，否则是多个线程共享
  extern __thread int t_cachedTid;  //线程真实pid(tid)的缓存，减少::syscall(SYS_gettid)调用次数
  extern __thread char t_tidString[32]; //tid的字符串表示形式
  extern __thread int t_tidStringLength; //上述字符串的长度
  extern __thread const char* t_threadName;//线程名称
  void cacheTid();  //缓存tid到t_cachedTid

  //返回线程tid
  inline int tid()
  {
      //如果未缓存过线程tid，则缓存tid到t_cachedTid
    if (__builtin_expect(t_cachedTid == 0, 0))
    {
      cacheTid();
    }
    return t_cachedTid;
  }

  inline const char* tidString() // for logging
  {
    return t_tidString;
  }

  inline int tidStringLength() // for logging
  {
    return t_tidStringLength;
  }

  inline const char* name()
  {
    return t_threadName;
  }

  bool isMainThread();//判断是不是主线程

  void sleepUsec(int64_t usec);//休眠
}
}

#endif
