// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_HTTP_HTTPREQUEST_H
#define MUDUO_NET_HTTP_HTTPREQUEST_H

#include <muduo/base/copyable.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Types.h>

#include <map>
#include <assert.h>
#include <stdio.h>

namespace muduo
{
namespace net
{

//http请求
class HttpRequest : public muduo::copyable
{
 public:
  enum Method   //请求方法
  {
    kInvalid, kGet, kPost, kHead, kPut, kDelete
  };
  enum Version   //请求方法
  {
    kUnknown, kHttp10, kHttp11
  };

  HttpRequest()
    : method_(kInvalid),
      version_(kUnknown)
  {
  }

  void setVersion(Version v)  //设置版本
  {
    version_ = v;
  }

  Version getVersion() const
  { return version_; }

  bool setMethod(const char* start, const char* end)  //设置方法
  {
    assert(method_ == kInvalid);
    string m(start, end);  //使用字符串首尾构造string，不包括尾部，如char *s="123", string s=(s,s+3),则s输出为123
    if (m == "GET")
    {
      method_ = kGet;
    }
    else if (m == "POST")
    {
      method_ = kPost;
    }
    else if (m == "HEAD")
    {
      method_ = kHead;
    }
    else if (m == "PUT")
    {
      method_ = kPut;
    }
    else if (m == "DELETE")
    {
      method_ = kDelete;
    }
    else
    {
      method_ = kInvalid;
    }
    return method_ != kInvalid;
  }

  Method method() const   //返回请求方法
  { return method_; }

  const char* methodString() const  //请求方法转换成字符串
  {
    const char* result = "UNKNOWN";
    switch(method_)
    {
      case kGet:
        result = "GET";
        break;
      case kPost:
        result = "POST";
        break;
      case kHead:
        result = "HEAD";
        break;
      case kPut:
        result = "PUT";
        break;
      case kDelete:
        result = "DELETE";
        break;
      default:
        break;
    }
    return result;
  }

  void setPath(const char* start, const char* end)  //设置路径 
  {
    path_.assign(start, end); //先将原字符串清空，再赋予新值
  }

  const string& path() const
  { return path_; }

  void setQuery(const char* start, const char* end)  //设置参数
  {
    query_.assign(start, end);
  }

  const string& query() const
  { return query_; }

  void setReceiveTime(Timestamp t)  //设置接收时间
  { receiveTime_ = t; }

  Timestamp receiveTime() const
  { return receiveTime_; }

//添加头部信息，客户传来一个字符串，我们把它转化成field: value的形式(头部字段表示：字段名称 冒号 空格 字段的值)
  void addHeader(const char* start, const char* colon, const char* end)
  {
    string field(start, colon);  //得到字段名称
    ++colon;
    while (colon < end && isspace(*colon))  //跳过空格
    {
      ++colon;
    }
    string value(colon, end);  //获取字段的值
    while (!value.empty() && isspace(value[value.size()-1]))  //去除右空格，如果右边有空格会一直resize-1
    {
      value.resize(value.size()-1);
    }
    headers_[field] = value;  //把对应字段对应值存储起来(std::map<string, string> headers_)
  }

  string getHeader(const string& field) const  //根据头部字段返回值内容
  {
    string result;
    std::map<string, string>::const_iterator it = headers_.find(field);
    if (it != headers_.end())
    {
      result = it->second;
    }
    return result;
  }

  const std::map<string, string>& headers() const    //返回头部列表
  { return headers_; }

  void swap(HttpRequest& that)  //交换HttpRequest内容
  {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    receiveTime_.swap(that.receiveTime_);
    headers_.swap(that.headers_);
  }

 private:
  Method method_; //请求方法
  Version version_; //协议版本1.0/1.1
  string path_; //请求路径
  string query_;  //请求参数
  Timestamp receiveTime_; //请求时间
  std::map<string, string> headers_;  //头部列表,存储 字段对应的值
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPREQUEST_H

/*
1、http request：

    request line + header + body （header分为普通报头，请求报头与实体报头）

    header与body之间有一空行（CRLF）

    请求方法有：

        Get, Post, Head, Put, Delete等


    协议版本1.0、1.1


    常用请求头

        Accept：浏览器可接受的媒体（MIME）类型；

        Accept-Language：浏览器所希望的语言种类

        Accept-Encoding：浏览器能够解码的编码方法，如gzip，deflate等

        User-Agent：告诉HTTP服务器， 客户端使用的操作系统和浏览器的名称和版本

        Connection：表示是否需要持久连接，Keep-Alive表示长连接，close表示短连接

*/

// 一个典型的http 请求：

// GET http://....  HTTP/1.1
// Accept: image/jpeg, application/x-ms-application, image/gif, application/xaml+xml, image/pjpeg, application/x-ms-xbap, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/msword, */*
// Accept-Language: zh-CN
// User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; Tablet PC 2.0)
// Accept-Encoding: gzip, deflate
// Host: 192.168.159.188:8000
// Connection: Keep-Alive
