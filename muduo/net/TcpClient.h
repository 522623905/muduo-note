// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/net/TcpConnection.h>

namespace muduo
{
namespace net
{

class Connector;
typedef boost::shared_ptr<Connector> ConnectorPtr;

/*
  Connector类不单独使用，它封装在类TcpClient中。一个Connector对应一个TcpClient，
  Connector用来建立连接，建立成功后把控制交给TcpConnection，因此TcpClient中也封装了一个TcpConnection。
  由于Connector,该客户端具备断开重连功能
*/

class TcpClient : boost::noncopyable
{
 public:
  // TcpClient(EventLoop* loop);
  // TcpClient(EventLoop* loop, const string& host, uint16_t port);
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const string& nameArg);
  ~TcpClient();  // force out-line dtor, for scoped_ptr members.

  void connect();  // 连接
  void disconnect();  // 断开连接
  void stop();  // 停止连接

  TcpConnectionPtr connection() const   // 返回TcpConnection对象
  {
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  EventLoop* getLoop() const { return loop_; }  // 获取所属的Reactor
  bool retry() const { return retry_; }   // 重连
  void enableRetry() { retry_ = true; }   // 允许重连  

  const string& name() const   // 获取名字
  { return name_; }

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setConnectionCallback(ConnectionCallback&& cb)
  { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback&& cb)
  { messageCallback_ = std::move(cb); }
  void setWriteCompleteCallback(WriteCompleteCallback&& cb)
  { writeCompleteCallback_ = std::move(cb); }
#endif

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd);   // 连接建立完毕会调用这个函数
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);   // 移除一个链接

  EventLoop* loop_;  // 所属的Reactor 
  ConnectorPtr connector_; // avoid revealing Connector  一个TcpClient有一个Connector对应,Connector用来建立连接
  const string name_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  bool retry_;   // atomic   //是否重连，是指建立的连接成功后又断开是否重连。而Connector的重连是一直不成功是否重试的意思
  bool connect_; // atomic   // 是否已经建立连接
  // always in loop thread
  int nextConnId_;          //name_+nextConnid_用于标识一个连接
  mutable MutexLock mutex_;
  TcpConnectionPtr connection_; // @GuardedBy mutex_  Connector建立连接成功后把控制交给TcpConnection
};

}
}

#endif  // MUDUO_NET_TCPCLIENT_H
