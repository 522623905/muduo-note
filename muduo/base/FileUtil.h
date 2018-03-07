// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_FILEUTIL_H
#define MUDUO_BASE_FILEUTIL_H

#include <muduo/base/StringPiece.h>
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace FileUtil
{

// read small file < 64KB
//小文件读取的类封装
class ReadSmallFile : boost::noncopyable
{
 public:
  ReadSmallFile(StringArg filename);
  ~ReadSmallFile();

  // return errno
  //该函数用于将小文件的内容转换为字符串
  template<typename String>
  int readToString(int maxSize,    //期望读取的大小
                   String* content, //要读入的content缓冲区
                   int64_t* fileSize, //读取出的整个文件大小
                   int64_t* modifyTime, //读取出的文件修改的时间
                   int64_t* createTime); //读取出的创建文件的时间

  /// Read at maxium kBufferSize into buf_
  // return errno
  int readToBuffer(int* size);//从文件读取数据到buf_

  const char* buffer() const { return buf_; }

  static const int kBufferSize = 64*1024;

 private:
  int fd_;
  int err_;
  char buf_[kBufferSize];
};

// read the file content, returns errno if error happens.
//一个全局函数，readFile函数，调用ReadSmallFile类中的readToString方法，供外部将小文件中的内容转化为字符串。
template<typename String>
int readFile(StringArg filename,
             int maxSize,
             String* content,   //把file内容保存到content变量
             int64_t* fileSize = NULL,
             int64_t* modifyTime = NULL,
             int64_t* createTime = NULL)
{
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

// not thread safe
//封装了一个文件指针的操作类,用于把数据写入文件中
class AppendFile : boost::noncopyable
{
 public:
  explicit AppendFile(StringArg filename);

  ~AppendFile();

  void append(const char* logline, const size_t len);

  void flush();

  size_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len);

  FILE* fp_; //文件指针
  char buffer_[64*1024]; //缓冲区，64K
  size_t writtenBytes_; //已经写入的字节数
};
}

}

#endif  // MUDUO_BASE_FILEUTIL_H

