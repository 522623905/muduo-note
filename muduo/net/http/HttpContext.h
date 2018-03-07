// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_HTTP_HTTPCONTEXT_H
#define MUDUO_NET_HTTP_HTTPCONTEXT_H

#include <muduo/base/copyable.h>

#include <muduo/net/http/HttpRequest.h>

namespace muduo
{
namespace net
{

class Buffer;

//这个类主要用于接收客户请求（这里为Http请求），并解析请求
class HttpContext : public muduo::copyable    
{
 public:
  enum HttpRequestParseState  //解析请求状态的枚举常量
  {
    kExpectRequestLine,  //当前正处于  解析请求行的状态
    kExpectHeaders,  //当前正处于  解析请求头部的状态
    kExpectBody,  //当前正处于  解析请求实体的状态
    kGotAll,   //解析完毕
  };

  HttpContext()
    : state_(kExpectRequestLine)   //初始状态，期望收到一个请求行
  {
  }

  // default copy-ctor, dtor and assignment are fine

  // return false if any error
  bool parseRequest(Buffer* buf, Timestamp receiveTime);  //处理请求

  //是否解析完毕
  bool gotAll() const
  { return state_ == kGotAll; }

  //重置HttpContext状态
  void reset()
  {
    state_ = kExpectRequestLine;
    HttpRequest dummy;  //构造一个临时空HttpRequest对象，
    request_.swap(dummy);//和当前的成员HttpRequest对象交换置空，然后临时对象析构
  }

  const HttpRequest& request() const  //返回request
  { return request_; }

  HttpRequest& request()
  { return request_; }

 private:
  bool processRequestLine(const char* begin, const char* end);  //解析请求行

  HttpRequestParseState state_;  // 请求解析状态
  HttpRequest request_;  // http请求
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPCONTEXT_H
