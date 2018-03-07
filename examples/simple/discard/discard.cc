#include "discard.h"

#include <muduo/base/Logging.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

DiscardServer::DiscardServer(EventLoop* loop,
                             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "DiscardServer")
{
  server_.setConnectionCallback(  //设置连接回调
      boost::bind(&DiscardServer::onConnection, this, _1)); //绑定的是成员函数，所以要有一个this
  server_.setMessageCallback( //设置消息回调
      boost::bind(&DiscardServer::onMessage, this, _1, _2, _3));  //绑定的是成员函数，所以要有一个this
}

void DiscardServer::start()
{
  server_.start();
}

void DiscardServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "DiscardServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
}

void DiscardServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer* buf,
                              Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " discards " << msg.size()
           << " bytes received at " << time.toString();
}

