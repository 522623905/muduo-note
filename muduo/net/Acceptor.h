// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
//类Acceptor用来listen、accept，并调用回调函数来处理新到的连接。
//客户不直接使用Acceptor，而是把它封装在TcpServer中用于获取新连接的fd
class Acceptor : boost::noncopyable
{
 public:
  typedef boost::function<void (int sockfd,
                                const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  //被用于在TcpServer构造函数中设置新连接处理回调
  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  bool listenning() const { return listenning_; } 
  void listen();  //该接口用来启动监听套接字

 private:
  void handleRead();  //处理新连接到来的私有函数

  EventLoop* loop_; //监听事件放在该loop循环中
  Socket acceptSocket_; //对监听套接字的封装
  Channel acceptChannel_; //对应的事件分发
  NewConnectionCallback newConnectionCallback_;//用户定义的连接回调函数
  bool listenning_; //是否正在监听状态
  int idleFd_;  //解决了服务器中文件描述符达到上限后如何处理的大问题!
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
