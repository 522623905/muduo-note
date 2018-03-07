// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_POLLER_EPOLLPOLLER_H
#define MUDUO_NET_POLLER_EPOLLPOLLER_H

#include <muduo/net/Poller.h>

#include <vector>

struct epoll_event;

namespace muduo
{
namespace net
{

///
/// IO Multiplexing with epoll(4).
///
class EPollPoller : public Poller     //注意，这里时继承了Poller类
{
 public:
  EPollPoller(EventLoop* loop);
  virtual ~EPollPoller();

  virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);  // 进行poll操作，timeoutMs 超时事件，activeChannels活动通道
  virtual void updateChannel(Channel* channel);  //更新通道
  virtual void removeChannel(Channel* channel);  //移除通道

 private:
  static const int kInitEventListSize = 16;  // EventList的初始空间大小 

  static const char* operationToString(int op);

  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;
  void update(int operation, Channel* channel);

  typedef std::vector<struct epoll_event> EventList;

  int epollfd_;  //文件描述符 = epoll_create1(EPOLL_CLOEXEC)，用来表示要  关注事件的fd的集合的描述符
  EventList events_;  // epoll_wait返回的活动的通道channelList
};

}
}
#endif  // MUDUO_NET_POLLER_EPOLLPOLLER_H
