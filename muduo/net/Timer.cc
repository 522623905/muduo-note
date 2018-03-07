// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Timer.h>

using namespace muduo;
using namespace muduo::net;

AtomicInt64 Timer::s_numCreated_;

void Timer::restart(Timestamp now)
{
    //如果设置重复，则重新计算下一个超时时刻
  if (repeat_)
  {
    expiration_ = addTime(now, interval_);
  }
  else
  {
      //如果不是重复定时，则另下一个时刻为非法时间即可
    expiration_ = Timestamp::invalid();
  }
}
