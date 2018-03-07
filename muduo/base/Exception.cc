// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Exception.h>

//#include <cxxabi.h>
#include <execinfo.h>
#include <stdlib.h>

using namespace muduo;

Exception::Exception(const char* msg)
  : message_(msg)
{
  fillStackTrace(); //获取栈回溯信息
}

Exception::Exception(const string& msg)
  : message_(msg)
{
  fillStackTrace();
}

Exception::~Exception() throw ()
{
}

const char* Exception::what() const throw()
{
  return message_.c_str();
}

const char* Exception::stackTrace() const throw()
{
  return stack_.c_str();
}

//获取栈信息
void Exception::fillStackTrace()
{
  const int len = 200;
  void* buffer[len]; //指针数组，来保存地址，最多保存len个
  int nptrs = ::backtrace(buffer, len);//将栈回溯信息（函数地址）保存到buffer当中
  char** strings = ::backtrace_symbols(buffer, nptrs);//将地址转换为函数名称（字符串要*,而指向多个字符串所以再*,因此**）
  if (strings)
  {
    for (int i = 0; i < nptrs; ++i)
    {
      // TODO demangle funcion name with abi::__cxa_demangle
      stack_.append(strings[i]);//依次把函数名称信息添加到stack
      stack_.push_back('\n');
    }
    free(strings); //因为实际是通过malloc函数开辟的内存，因此用完要释放
  }
}

