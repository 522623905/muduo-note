// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_INSPECT_PROCESSINSPECTOR_H
#define MUDUO_NET_INSPECT_PROCESSINSPECTOR_H

#include <muduo/net/inspect/Inspector.h>
#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

//实际实现的时候通过ProcessInfo类返回进程信息
class ProcessInspector : boost::noncopyable
{
 public:
  void registerCommands(Inspector* ins); // 注册命令接口

  //向外部提供的四个回调函数，返回服务器进程的相关信息
  static string overview(HttpRequest::Method, const Inspector::ArgList&);
  static string pid(HttpRequest::Method, const Inspector::ArgList&);
  static string procStatus(HttpRequest::Method, const Inspector::ArgList&);
  static string openedFiles(HttpRequest::Method, const Inspector::ArgList&);
  static string threads(HttpRequest::Method, const Inspector::ArgList&);

  static string username_;
};

}
}

#endif  // MUDUO_NET_INSPECT_PROCESSINSPECTOR_H
