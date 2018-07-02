// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
  : loop_(CHECK_NOTNULL(loop)), //检查loop非空指针
    ipPort_(listenAddr.toIpPort()), //IP:端口号
    name_(nameArg),  //名称
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), //Accept的封装,用于获取新连接的fd,供TcpServer使用
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)  //记录连接数，当有新连接的时候会自增
{
    //Acceptor::handleRead函数中会调用TcpServer::newConnection
    //_1对应的是socket文件描述符，_2对应的是对等方的地址InetAddress
  acceptor_->setNewConnectionCallback(
      boost::bind(&TcpServer::newConnection, this, _1, _2));
}

/*
当TcpServer对象析构时，就会关闭所有连接。
*/
TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (ConnectionMap::iterator it(connections_.begin());  //在析构函数中销毁connection
      it != connections_.end(); ++it)
  {
    TcpConnectionPtr conn = it->second;
    it->second.reset();
    conn->getLoop()->runInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));
    conn.reset();
  }
}

void TcpServer::setThreadNum(int numThreads)  //设置线程池大小
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

//启动Server，监听客户连接
void TcpServer::start() 
{
  if (started_.getAndSet(1) == 0)
  {
    threadPool_->start(threadInitCallback_);  //启动线程池

    assert(!acceptor_->listenning());
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));  //执行accept的listen监听连接
  }
}

/*
当新建连接到达后，TcpServer创建一个新的TcpConnection对象来保存这个连接，
设置这个新连接的回调函数，之后在EventLoopThreadPool中取一个EventLoop对象
来作为这个新连接的reactor
*/
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop(); //在线程池取一个EventLoop对象
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);  //生成这个连接的名字
  ++nextConnId_;  //连接数+1
  string connName = name_ + buf;  //组合成连接的一个名字

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd)); //由sockfd获取sockaddr_in结构
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary 
  TcpConnectionPtr conn(new TcpConnection(ioLoop,           //conn用来管理已连接套接字
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  connections_[connName] = conn;  //map,name与conn绑定,conn引用计数变为2
  conn->setConnectionCallback(connectionCallback_); //原样把各个回调函数传给TcpConnectionPtr
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe

  //由于ioloop所属的IO线程与当前线程不是同一个线程,不能直接调用
  //要转到ioLoop所属的线程进行调用,因此用runInLoop
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn)); //将新到来的连接加入到监听事件中
}
//执行完毕后,conn引用计数减为1

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());  //根据connection的名字移除connection,则析构，断开连接
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));
}

