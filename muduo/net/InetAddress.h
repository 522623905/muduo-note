// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_INETADDRESS_H
#define MUDUO_NET_INETADDRESS_H

#include <muduo/base/copyable.h>
#include <muduo/base/StringPiece.h>

#include <netinet/in.h>

namespace muduo
{
namespace net
{
namespace sockets
{
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
}

///
/// Wrapper of sockaddr_in.
///
/// This is an POD interface class.
/*
 * 网络地址类
 * 只是sockaddr_in的一个包装，实现了各种针对sockaddr_in的操作
 */
class InetAddress : public muduo::copyable
{
 public:
  /// Constructs an endpoint with given port number.
  /// Mostly used in TcpServer listening.
  explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

  /// Constructs an endpoint with given ip and port.
  /// @c ip should be "1.2.3.4"
  InetAddress(StringArg ip, uint16_t port, bool ipv6 = false);

  /// Constructs an endpoint with given struct @c sockaddr_in
  /// Mostly used when accepting new connections
  ///  用sockaddr_in对象构造地址
  explicit InetAddress(const struct sockaddr_in& addr)
    : addr_(addr)
  { }

  explicit InetAddress(const struct sockaddr_in6& addr)
    : addr6_(addr)
  { }

  sa_family_t family() const { return addr_.sin_family; }
  string toIp() const;// 返回ip
  string toIpPort() const;// 返回ip和端口的字符串
  uint16_t toPort() const;// 返回端口

  // default copy/assignment are Okay

  const struct sockaddr* getSockAddr() const { return sockets::sockaddr_cast(&addr6_); }// 返回sockaddr_in对象
  void setSockAddrInet6(const struct sockaddr_in6& addr6) { addr6_ = addr6; }// 设置sockaddr_in对象

  uint32_t ipNetEndian() const; // 返回网络字节顺序的ip
  uint16_t portNetEndian() const { return addr_.sin_port; }// 返回网络字节顺序的端口

  // resolve hostname to IP address, not changing port or sin_family
  // return true on success.
  // thread safe
  static bool resolve(StringArg hostname, InetAddress* result);// 把主机名转换成ip地址，线程安全
  // static std::vector<InetAddress> resolveAll(const char* hostname, uint16_t port = 0);

 private:
  union
  {
    struct sockaddr_in addr_;
    struct sockaddr_in6 addr6_;
  };
};

}
}

#endif  // MUDUO_NET_INETADDRESS_H
