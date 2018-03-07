// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_INSPECT_INSPECTOR_H
#define MUDUO_NET_INSPECT_INSPECTOR_H

#include <muduo/base/Mutex.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpServer.h>

#include <map>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class ProcessInspector;
class PerformanceInspector;
class SystemInspector;

// An internal inspector of the running process, usually a singleton.
// Better to run in a separated thread, as some method may block for seconds
class Inspector : boost::noncopyable
{
 public:
  typedef std::vector<string> ArgList;
  typedef boost::function<string (HttpRequest::Method, const ArgList& args)> Callback;
  Inspector(EventLoop* loop,
            const InetAddress& httpAddr,
            const string& name);
  ~Inspector();

  // Add a Callback for handling the special uri : /mudule/command
  // 添加监控器对应的回调函数
  // 如add("proc", "pid", ProcessInspector::pid, "print pid");  
  // http://192.168.159.188:12345/proc/pid这个http请求就会相应的调用ProcessInspector::pid来处理
  void add(const string& module,    //模块
           const string& command,   //命令
           const Callback& cb,      //回调执行方法
           const string& help);     //帮助信息
  void remove(const string& module, const string& command); //移除模块的命令

 private:
  typedef std::map<string, Callback> CommandList;  //命令列表，对于客端发起的每个命令，有一个回调函数处理
  typedef std::map<string, string> HelpList;    //针对客端命令的帮助信息列表

  void start();
  void onRequest(const HttpRequest& req, HttpResponse* resp);

  HttpServer server_; //包含一个HttpServer对象
  boost::scoped_ptr<ProcessInspector> processInspector_;    //暴露的接口，进程模块
  boost::scoped_ptr<PerformanceInspector> performanceInspector_;    //性能模块
  boost::scoped_ptr<SystemInspector> systemInspector_;  //系统模块
  MutexLock mutex_;
  std::map<string, CommandList> modules_;   //模块,对应module -- command -- callback
  std::map<string, HelpList> helps_;    //帮助，对应于module -- command -- help
};

}
}

#endif  // MUDUO_NET_INSPECT_INSPECTOR_H
