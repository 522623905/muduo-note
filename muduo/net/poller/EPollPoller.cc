// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/EPollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>

using namespace muduo;
using namespace muduo::net;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

namespace       //匿名的namespace，只在本文件有效
{
const int kNew = -1;  //表示有新通道要增加
const int kAdded = 1;  //要关注的通道
const int kDeleted = 2;  //将已不关注事件的fd重新关注事件
}

EPollPoller::EPollPoller(EventLoop* loop)
  : Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),  //表示生成的epoll fd具有“执行exec函数后关闭”特性
    events_(kInitEventListSize)  //vector这样用时初始化kInitEventListSize个大小空间
{
  if (epollfd_ < 0)
  {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EPollPoller::~EPollPoller()
{
  ::close(epollfd_);
}

//使用epoll_wait等待事件到来，并把到来的事件填充至activeChannels
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  LOG_TRACE << "fd total count " << channels_.size();
  int numEvents = ::epoll_wait(epollfd_,
                               &*events_.begin(),
                               static_cast<int>(events_.size()),
                               timeoutMs);         //使用epoll_wait()，等待事件返回,返回发生的事件数目
  int savedErrno = errno;  //错误号
  Timestamp now(Timestamp::now());  //得到时间戳
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels); //把发生的numEvents事件个填充给活跃事件通道表activeChannels中
    if (implicit_cast<size_t>(numEvents) == events_.size())  //如果返回的事件数目等于当前事件数组大小，就再分配2倍空间
    {
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0)  //没有事件发生，只是超时返回
  {
    LOG_TRACE << "nothing happended";
  }
  else
  {
    // error happens, log uncommon ones
    if (savedErrno != EINTR)  //如果不是EINTR信号，就把错误号保存下来，并且输入到日志中
    {
      errno = savedErrno;
      LOG_SYSERR << "EPollPoller::poll()";
    }
  }
  return now;  //返回时间戳 
}

//把发生的numEvents个事件填充给活跃事件通道表activeChannels中
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
  assert(implicit_cast<size_t>(numEvents) <= events_.size());  //确定它的大小小于events_的大小，因为events_是预留的事件vector
  for (int i = 0; i < numEvents; ++i)   //挨个处理发生的numEvents个事件，epoll模式返回的events_数组中都是已经发生额事件，这有别于select和poll
  {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG                   //如果是调试状态，则
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    channel->set_revents(events_[i].events);  //填充已发生的事件
    activeChannels->push_back(channel);  //把该channel添加进当前通道活动列表activeChannels
  }
}
/* 
这是epoll模式epoll_event事件的数据结构，其中data不仅可以保存fd，也可以保存一个void*类型的指针。 
typedef union epoll_data { 
               void    *ptr; 
               int      fd; 
               uint32_t u32; 
               uint64_t u64; 
           } epoll_data_t; 
 
           struct epoll_event { 
               uint32_t     events;    // Epoll events  
               epoll_data_t data;      //User data variable  
           }; 
*/  


//这个函数被调用是因为channel->enableReading()等被调用，再调用channel->update()，
//再EventLoop->updateChannel()，再->epoll或poll的updateChannel被调用  
//函数：更新通道,将channel对应的fd事件注册到epoll内核时间表中     
void EPollPoller::updateChannel(Channel* channel)  
{
  Poller::assertInLoopThread();
  const int index = channel->index();  //获得channel的index，初始状态index是-1
  LOG_TRACE << "fd = " << channel->fd()
    << " events = " << channel->events() << " index = " << index;
  // kNew 表示有新的通道要增加，kDeleted表示将已不关注事件的fd重新关注事件，及时重新加到epollfd_中去
  if (index == kNew || index == kDeleted)
  {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew)  //新通道
    {
      assert(channels_.find(fd) == channels_.end()); //断言如果是新的channel，那么在channels_里面是找不到的
      channels_[fd] = channel;  //则把该新channel加入channels_
    }
    else // index == kDeleted   
    {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }

    channel->set_index(kAdded); //设置状态为已关注
    update(EPOLL_CTL_ADD, channel);  //把该channel对应的事件关注到epollfd_中
  }
  //如果是已经存在的关注的通道
  else
  {
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);  //断言已经在channels_里面了，并且已在epollfd_中
    assert(index == kAdded);
    if (channel->isNoneEvent())    //如果channel没有事件关注了，就把他从epollfd_中剔除掉，并更新index
    {
      update(EPOLL_CTL_DEL, channel); //使用EPOLL_CTL_DEL从内核事件表中删除
      channel->set_index(kDeleted);  //删除之后设为deleted，表示已经删除，只是从内核事件表中删除，在channels_这个通道数组中并没有删除
    }
    else  //如果仍然有关注，那就只是更新。更新成什么样子channel中会决定。
    {
      update(EPOLL_CTL_MOD, channel); 
    }
  }
}

// 从channels_里面移除channel，从epollfd_删除channel对应的fd
void EPollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  int fd = channel->fd();
  LOG_TRACE << "fd = " << fd;
  assert(channels_.find(fd) != channels_.end()); //断言能在channels_里面找到channel
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent()); //断言所要移除的channel已经没有事件关注了，但是此时在event_里面可能还有他的记录
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  size_t n = channels_.erase(fd); //真正从channels_里面删除掉channel 
  (void)n;
  assert(n == 1);

  if (index == kAdded)  //如果还在关注中，则从epollfd_中删除channel对应的fd
  {
    update(EPOLL_CTL_DEL, channel); 
  }
  channel->set_index(kNew);  // 被从channels移除了，所以channel状态现在变成新的了
}

//epoll_ctl函数的封装。在epollfd_中更新channel对应的fd事件
void EPollPoller::update(int operation, Channel* channel)
{
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();
  event.data.ptr = channel; //把channel传入data.ptr（void *类型）
  int fd = channel->fd();
  LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
    << " fd = " << fd << " event = { " << channel->eventsToString() << " }";
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) //更新该epollfd中的fd描述符对应的操作
  {
    if (operation == EPOLL_CTL_DEL)
    {
      LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
    }
    else
    {
      LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
    }
  }
}

const char* EPollPoller::operationToString(int op)  //调试用的函数
{
  switch (op)
  {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_DEL:
      return "DEL";
    case EPOLL_CTL_MOD:
      return "MOD";
    default:
      assert(false && "ERROR op");
      return "Unknown Operation";
  }
}
