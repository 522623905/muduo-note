// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Buffer.h>

#include <muduo/net/SocketsOps.h>

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536]; //从fd读数据到Buffer中时，因为不知道一次性可以读多少，因此在栈上开辟了65536字节的空间extrabuf，使用readv读取
  struct iovec vec[2];    
  const size_t writable = writableBytes();  //Buffer中wriable足够存放从fd读到的数据，则读取完毕；否则剩余数据读到extrabuf中，再将其添加到Buffer中。
  vec[0].iov_base = begin()+writerIndex_; //内存1起始地址
  vec[0].iov_len = writable;  //这块内存1长度
  vec[1].iov_base = extrabuf; //内存2起始地址
  vec[1].iov_len = sizeof extrabuf; //这块内存2长度
  // when there is enough space in this buffer, don't read into extrabuf. 
  // when extrabuf is used, we read 128k-1 bytes at most.   
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = sockets::readv(fd, vec, iovcnt);  //从fd中读取内容到vec内存块
  if (n < 0)
  {
    *savedErrno = errno;
  }
  else if (implicit_cast<size_t>(n) <= writable)  //Buffer中可以存储所有读到的数据
  {
    writerIndex_ += n;
  }
  else  //读的数据太多，部分先存储到栈extrabuf,再添加到Buffer中
  {
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable); //把extrabuf写入到Buffer中
  }
  // if (n == writable + sizeof extrabuf)
  // {
  //   goto line_30;
  // }
  return n;
}
            
