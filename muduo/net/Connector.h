// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include <muduo/net/InetAddress.h>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;

/*
***Connector用来非阻塞主动发起发起连接,带有重连功能.
***该类不单独使用，而是放于TcpClient中使用
*/
class Connector : boost::noncopyable,
                  public boost::enable_shared_from_this<Connector>
{
 public:
  typedef boost::function<void (int sockfd)> NewConnectionCallback;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  void start();  // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();  // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  //未连接状态,正在连接(中间状态),已连接,
  enum States { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30*1000;  //最大重试延迟
  static const int kInitRetryDelayMs = 500;  //初始化重试延迟

  void setState(States s) { state_ = s; }
  void startInLoop(); 
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

  EventLoop* loop_;  //所属的EventLoop
  InetAddress serverAddr_;  //server地址
  bool connect_; // atomic
  States state_;  // FIXME: use atomic variable
  boost::scoped_ptr<Channel> channel_;  //Connector所对应的Channel
  NewConnectionCallback newConnectionCallback_; //连接成功回调函数
  int retryDelayMs_;  //重连延迟时间(单位ms)
};

}
}

#endif  // MUDUO_NET_CONNECTOR_H
