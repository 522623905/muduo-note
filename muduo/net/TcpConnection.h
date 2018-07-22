// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.

//和新连接相关的所有内容统一封装到该类
//继承了enable_shared_from_this类，保证返回的对象时shared_ptr类型
//生命期依靠shared_ptr管理（即用户和库共同控制）
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }
  bool connected() const { return state_ == kConnected; } //判断是连接建立还是断开
  bool disconnected() const { return state_ == kDisconnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;
  string getTcpInfoString() const;

  // void send(string&& message); // C++11
  void send(const void* message, int len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);
  // reading or not
  void startRead();
  void stopRead();
  bool isReading() const { return reading_; } // NOT thread safe, may race with start/stopReadInLoop

  void setContext(const boost::any& context)  //boost::any是一个能保存任意类型值的类
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  /// Advanced interface
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  Buffer* outputBuffer()
  { return &outputBuffer_; }

  /// Internal use only.
  /// 这是给TcpServer和TcpClient用的,不是给用户用的
  /// 普通用户用的是ConnectionCallback
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  //枚举了每个TcpConnection对应的几种状态(状态转移参考书籍P317)
  //相关函数：connectEstablished()  shutdown()  handleClose()
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime); //处理读事件
  void handleWrite(); //处理写事件
  void handleClose(); //处理关闭事件       (这些处理函数都是要传给TcpConnection对应的Channel的)
  void handleError(); //处理错误事件
  // void sendInLoop(string&& message);
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();  //用于主动关闭连接
  void setState(StateE s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  EventLoop* loop_; //TcpConnection所属的loop
  const string name_;//连接名称
  StateE state_;  // FIXME: use atomic variable
  bool reading_;
  // we don't expose those classes to client.
  boost::scoped_ptr<Socket> socket_;  //RAII套接字对象
  boost::scoped_ptr<Channel> channel_; //套接字上对应的事件以及处理都将由和套接字对应的Channel来处理
  const InetAddress localAddr_; //本地服务器地址
  const InetAddress peerAddr_;  //对方客户端地址
  ConnectionCallback connectionCallback_; //连接回调
  MessageCallback messageCallback_; //接收消息到达时的回调
  WriteCompleteCallback writeCompleteCallback_; // 写完成回调
  HighWaterMarkCallback highWaterMarkCallback_; //outbuffer快满了的高水位回调函数 
  CloseCallback closeCallback_;   // 内部的close回调函数
  size_t highWaterMark_;  //发送缓冲区数据“上限阀值”，超过这个值
   //每一个连接都会对应一对读写input/output buffer
  //muduo保证了在操作时,Buffer必是线程安全的
  //对于 input buffer，onMessage() 回调始终发生在该 TcpConnection 所属的那个 IO 线程
  //TcpConnection::send() 来发送数据，outputBuffer是线程安全的
  Buffer inputBuffer_; //保存读取到的sockfd中的数据
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer. 当send无法一次性发送完数据后,会先暂存到这里,等下次发送
  boost::any context_;  // boost库的any 可以保持任意的类型 绑定一个未知类型的上下文对象
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
};

typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
