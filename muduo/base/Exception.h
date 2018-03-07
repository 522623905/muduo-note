// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_EXCEPTION_H
#define MUDUO_BASE_EXCEPTION_H

#include <muduo/base/Types.h>
#include <exception>

namespace muduo
{

//带stack trace的异常基类
class Exception : public std::exception
{
 public:
    //构造函数中会获取栈回溯信息
  explicit Exception(const char* what);
  explicit Exception(const string& what);
  virtual ~Exception() throw();
  virtual const char* what() const throw();
  const char* stackTrace() const throw();

 private:
  void fillStackTrace();//获取栈信息

  string message_; //异常信息的字符串
  string stack_;  //保存异常发生时的栈回溯信息
};

}

#endif  // MUDUO_BASE_EXCEPTION_H
