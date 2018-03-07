// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),  //初始化创建sockt fd
    acceptChannel_(loop, acceptSocket_.fd()),  //初始化channel与sockfd绑定
    listenning_(false), //初始化未监听
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))  //打开一个空洞文件(/dev/null)后返回一空闲的文件描述符！
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true); //不用等待Time_Wait状态结束
  acceptSocket_.setReusePort(reuseport);
  acceptSocket_.bindAddress(listenAddr);//bind
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this));  //当fd可读时调用回调函数hanleRead
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();  //将其从poller监听集合中移除，此时为kDeleted状态
  acceptChannel_.remove();  //将其从EventList events_中移除，此时为kNew状态
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading(); //listen完毕才使能读事件
}

void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr;
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr); //这里是真正接收连接
  if (connfd >= 0)  //接受连接成功，则执行连接回调
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr); //将新连接信息传送到用户定义的回调函数中
    }
    else  //没有回调函数则关闭client对应的fd
    {
      sockets::close(connfd);
    }
  }
  else  //接收套接字失败
  {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)  //返回的错误码为EMFILE(超过最大连接数)
    {
      ::close(idleFd_); //把提前拥有的idleFd_关掉，这样就腾出来文件描述符来接受新连接了
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_); //接受到新连接之后将其关闭，然后我们就再次获得一个空洞文件描述符保存到idleFd_中
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

/*
idleFd_作用防止busy loop：
如果此时你的最大连接数达到了上限，而accept队列里可能还一直在增加新的连接等你接受，
muduo用的是epoll的LT模式时，那么如果因为你连接达到了文件描述符的上限，
此时没有可供你保存新连接套接字描述符的文件符了，
那么新来的连接就会一直放在accept队列中，于是呼其对应的可读事件就会
一直触发读事件(因为你一直不读，也没办法读走它)，此时就会造成我们常说的busy loop
*/
