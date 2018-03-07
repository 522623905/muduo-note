// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Buffer.h>
#include <muduo/net/http/HttpContext.h>

using namespace muduo;
using namespace muduo::net;
 
//解析请求行  格式 : GET http://....  HTTP/1.1
bool HttpContext::processRequestLine(const char* begin, const char* end)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');  //查找空格
  if (space != end && request_.setMethod(start, space))  //找到GET并设置请求方法
  {
    start = space+1;  //移动位置
    space = std::find(start, end, ' '); //再次查找空格
    if (space != end)  //找到
    {
      const char* question = std::find(start, space, '?');
      if (question != space)  //找到了'?'，说明有请求参数
      {
        request_.setPath(start, question);  //设置路径
        request_.setQuery(question, space); //设置请求参数
      }
      else  //没有请求参数
      {
        request_.setPath(start, space);  //设置路径
      }
      start = space+1;//移动位置
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");  //查找有没有"HTTP/1."
      if (succeed)  //如果成功，判断是采用HTTP/1.1还是HTTP/1.0
      {
        if (*(end-1) == '1')
        {
          request_.setVersion(HttpRequest::kHttp11);
        }
        else if (*(end-1) == '0')
        {
          request_.setVersion(HttpRequest::kHttp10);
        }
        else
        {
          succeed = false;  //若没有指明http版本号，请求行失败
        }
      }
    }
  }
  return succeed;
}

// return false if any error
//处理请求，利用状态机编程
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
  bool ok = true;
  bool hasMore = true;
  while (hasMore)
  {
      //初始状态是处于解析请求行的状态，下一次循环不是该状态就不会进入，一般只进入一次
    if (state_ == kExpectRequestLine)
    {
      const char* crlf = buf->findCRLF();  //首先查找\r\n，就会到GET / HTTP/1.1的请求行末尾 
      if (crlf)  
      {
        ok = processRequestLine(buf->peek(), crlf); //解析请求行
        if (ok)  //如果成功，设置请求行事件
        {
          request_.setReceiveTime(receiveTime); //设置解析请求行完毕的时间
          buf->retrieveUntil(crlf + 2);  //移动buf偏移量(将请求行从buf中取出，包括\r\n)
          state_ = kExpectHeaders;  //将Httpontext状态改为KexpectHeaders状态
        }
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
    //处于要解析Header状态
    else if (state_ == kExpectHeaders)
    {
      const char* crlf = buf->findCRLF();  //查找\r\n位置
      if (crlf)
      {
        const char* colon = std::find(buf->peek(), crlf, ':');  //查找:(请求头形式： 字段名: 具体值)
        if (colon != crlf)
        {
          request_.addHeader(buf->peek(), colon, crlf);  //找到添加头部，加到map容器 
        }
        else
        {
          // empty line, end of header
          // FIXME:
          state_ = kGotAll;  //一旦请求完毕，再也找不到':'了，状态改为gotall状态，循环退出
          hasMore = false;
        }
        buf->retrieveUntil(crlf + 2);  //请求完毕,移动Buffer索引位置
      }
      else
      {
        hasMore = false;
      }
    }
    else if (state_ == kExpectBody)
    {
      // FIXME:
    }
  }
  return ok;
}
