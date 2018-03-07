// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/http/HttpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/http/HttpContext.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpResponse.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

namespace muduo
{
namespace net
{
namespace detail
{

//默认HTTP回调，返回错误码
void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
  resp->setStatusCode(HttpResponse::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

}
}
}

//初始化TcpServer，并将HttpServer的回调函数传给TcpServer
HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& name,
                       TcpServer::Option option)
  : server_(loop, listenAddr, name, option),
    httpCallback_(detail::defaultHttpCallback)
{
  server_.setConnectionCallback(
      boost::bind(&HttpServer::onConnection, this, _1));  //连接到来回调该函数
  server_.setMessageCallback(
      boost::bind(&HttpServer::onMessage, this, _1, _2, _3));  //消息到来回调该函数
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
  LOG_WARN << "HttpServer[" << server_.name()
    << "] starts listenning on " << server_.ipPort();
  server_.start();
}

//新连接回调
void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    //构造一个http上下文对象，用来解析http请求，利用boost::any保存至TcpConnection上下文中
    conn->setContext(HttpContext());  
  }
}

//消息回调
//1.解析http请求
//2.处理http请求
void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());  //取出请求，mutable可以改变

  if (!context->parseRequest(buf, receiveTime))  //调用context的parseRequest解析请求，判断请求是否合法
  {
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");  //失败，发送400通用客户请求错误
    conn->shutdown();  //关闭连接
  }

  if (context->gotAll())  //判断是否解析http请求完毕
  {
    onRequest(conn, context->request());  //调用onRequest来响应对应的请求
    context->reset();  //一旦请求处理完毕，重置context，因为HttpContext和TcpConnection绑定了，我们需要解绑重复使用
  }
}

//根据http请求，进行相应处理
void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
  const string& connection = req.getHeader("Connection"); //取出头部Connection对应的内容
  bool close = connection == "close" ||
    (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive"); // 如果connection为close或者1.0版本不支持keep-alive，标志着我们处理完请求要关闭连接 
  HttpResponse response(close); //使用close构造一个HttpResponse对象，该对象可以通过方法.closeConnection()判断是否关闭连接 
  httpCallback_(req, &response);  //执行用户注册的回调函数来填充httpResponse
  Buffer buf;
  response.appendToBuffer(&buf);  //用户填充httpResponse后的信息，追加到缓冲区 buf中
  conn->send(&buf); //把缓冲数据发送给客户端
  if (response.closeConnection()) //判断响应是否设置了关闭
  {
    conn->shutdown(); //关了它
  }
}

