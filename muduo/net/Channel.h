// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
//
/*
负责事件的分发:
最起码得拥有的数据成员有，分发哪个文件描述符上的事件
即上面数据成员中的fd_，其次得知道该poller监控该文件描
述符上的哪些事件，对应events_，接着就是当该文件描述符
就绪之后其上发生了哪些事件对应上面的revents。知道了发
生了什么事件，要达到事件分发的功能你总得有各个事件的处
理回调函数把，对应上面的各种callback。最后该Channel由哪
个loop_监控并处理的loop_
*/
class Channel : boost::noncopyable
{
 public:
  typedef boost::function<void()> EventCallback;
  typedef boost::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);  //是Channel最核心的接口真正实现事件分发的函数
  
  //以下接口便是设置各种事件对应的回调函数
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setReadCallback(ReadEventCallback&& cb)
  { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback&& cb)
  { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback&& cb)
  { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback&& cb)
  { errorCallback_ = std::move(cb); }
#endif

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; } // used by pollers 该接口用来设置Poller需要监听Channel的哪些事件
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); }
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  // for Poller
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // for debug
  string reventsToString() const;
  string eventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  static string eventsToString(int fd, int ev);

  void update(); //更新channel
  void handleEventWithGuard(Timestamp receiveTime); //会根据revents触发的事件来分别决定调用哪些回调 

  static const int kNoneEvent;    //static常量定义，用“类::XX”初始化方式
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_; // 属于哪一个Reactor
  const int  fd_; // 关联fd
  int        events_;   //用户设置关心的IO事件
  int        revents_; // it's the received event types of epoll or poll  目前的活动事件，由EventLoop/Poller设置
  int        index_; // used by Poller. 在Poller中的编号(poll事件数组的序号或epoll的通道状态)，构造函数初始化-1
  bool       logHup_;

  boost::weak_ptr<void> tie_; // 绑定的对象,用处?
  bool tied_;  // 是否绑定了对象上来
  bool eventHandling_; // 当前是否正在处理event
  bool addedToLoop_;//是否添加了channel到loop中
  ReadEventCallback readCallback_; //读回调
  EventCallback writeCallback_; // 定义如何写数据
  EventCallback closeCallback_;  // 定义如何关闭连接
  EventCallback errorCallback_; // 定义如果出错的话如何处理
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
