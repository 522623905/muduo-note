// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/base/FileUtil.h>
#include <muduo/base/Logging.h> // strerror_tl

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

using namespace muduo;

//不是线程安全的
FileUtil::AppendFile::AppendFile(StringArg filename)
  : fp_(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC
    writtenBytes_(0)
{
  assert(fp_);
  ::setbuffer(fp_, buffer_, sizeof buffer_);//设置文件指针fp_的缓冲区设定64K，也就是文件的stream大小
  // posix_fadvise POSIX_FADV_DONTNEED ?
}

FileUtil::AppendFile::~AppendFile()
{
  ::fclose(fp_);
}

//不是线程安全的，需要外部加锁
void FileUtil::AppendFile::append(const char* logline, const size_t len)
{
  size_t n = write(logline, len); //返回的n是已经写入文件的字节数
  size_t remain = len - n; //相减大于0表示未写完
  while (remain > 0)
  {
    size_t x = write(logline + n, remain); //同样x是已经写的字节数
    if (x == 0)
    {
      int err = ferror(fp_);
      if (err)
      {
        fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
      }
      break;
    }
    n += x; //偏移
    remain = len - n; // remain -= x
  }

  writtenBytes_ += len; //已经写入的个数
}

void FileUtil::AppendFile::flush()
{
  ::fflush(fp_); //刷新流
}

size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
  // #undef fwrite_unlocked
  return ::fwrite_unlocked(logline, 1, len, fp_);//不加锁的方式写入，效率高，not thread safe
}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
  : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}

FileUtil::ReadSmallFile::~ReadSmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

// return errno
//模板函，是RredSmallFile类中的读取到字符串string类型
template<typename String>
int FileUtil::ReadSmallFile::readToString(int maxSize,
                                          String* content,
                                          int64_t* fileSize,
                                          int64_t* modifyTime,
                                          int64_t* createTime)
{
  BOOST_STATIC_ASSERT(sizeof(off_t) == 8);
  assert(content != NULL);
  int err = err_;
  if (fd_ >= 0)
  {
    content->clear();

    if (fileSize)//如果不为空，获取文件大小
    {
      struct stat statbuf;
      if (::fstat(fd_, &statbuf) == 0)//fstat函数用来 获取文件的属性,保存到缓冲区当中
      {
        if (S_ISREG(statbuf.st_mode)) //S_ISREG判断是否是一个常规文件
        {
          *fileSize = statbuf.st_size; //获取文件大小,给到输入参数
          content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));//给content预留的空间大小
        }
        else if (S_ISDIR(statbuf.st_mode)) //S_ISDIR判断是否是目录
        {
          err = EISDIR;
        }
        if (modifyTime)
        {
          *modifyTime = statbuf.st_mtime; //获取文件的修改时间
        }
        if (createTime)
        {
          *createTime = statbuf.st_ctime; //获取文件的创建时间
        }
      }
      else
      {
        err = errno;
      }
    }

    while (content->size() < implicit_cast<size_t>(maxSize))
    {
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      ssize_t n = ::read(fd_, buf_, toRead);//从文件当中读取数据到字符串buf_
      if (n > 0)
      {
        content->append(buf_, n);//从buf_追加n个字节到字符串content
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

//从文件读取数据到buf_
int FileUtil::ReadSmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    //pread和read区别，pread读取完文件offset不会更改;而read会引发offset随读到的字节数移动
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0); //返回已读的字节数
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

//对成员函数进行模板的显示实例化，提高效率
template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

#ifndef MUDUO_STD_STRING
template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                std::string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    std::string* content,
    int64_t*, int64_t*, int64_t*);
#endif

