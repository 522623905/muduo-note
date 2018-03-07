// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Connector.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;


//构造函数初始化了I/O线程，服务器地址，并设置为未连接状态以及初始化了重连延时时间
Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)  //初始化延时
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

void Connector::start()
{
  connect_ = true;
  loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

void Connector::startInLoop() //在当前IO中建立连接
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
    connect();  //开始建立连接
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop()
{
  connect_ = false;
  loop_->queueInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
  // FIXME: cancel timer
}

void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();
    retry(sockfd);
  }
}

void Connector::connect()   //开始建立连接
{
  int sockfd = sockets::createNonblockingOrDie(serverAddr_.family()); //创建非阻塞sockfd
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());  //连接服务器
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)  //检查错误码
  {
    case 0:
    case EINPROGRESS: //非阻塞套接字，未连接成功返回码是EINPROGRESS表示正在连接
    case EINTR:
    case EISCONN:     //连接成功
      connecting(sockfd);
      break;

    case EAGAIN:  //这是真的错误，表面本机port暂时用完，要关闭sockfd再延期重试
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);  //重连
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);  //这几种情况不能重连
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}

void Connector::restart()   //重启
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}


//如果连接成功,即errno为EINPROGRESS、EINTR、EISCONN
//连接成功就是更改连接状态+设置各种回调函数+加入poller关注可写事件
void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);  
  channel_.reset(new Channel(loop_, sockfd)); //Channel与sockfd关联
  channel_->setWriteCallback(         //设置可写回调函数，这时候如果socket没有错误，sockfd就处于可写状态
      boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe
  channel_->setErrorCallback(           //设置错误回调函数
      boost::bind(&Connector::handleError, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  channel_->enableWriting();  //关注写事情。在非阻塞中，当sockfd变得可写时表明连接建立完毕
}

int Connector::removeAndResetChannel() //移除channel。Connector中的channel只管理建立连接阶段。连接建立后，交给TcoConnection管理
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

void Connector::resetChannel()  //reset后channel_为空
{
  channel_.reset(); //显式销毁它们所管理的对象
}

void Connector::handleWrite()  //可写不一定表示已经建立连接
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel(); //移除channel。Connector中的channel只管理建立连接阶段。连接建立后，交给TcoConnection管理
    int err = sockets::getSocketError(sockfd); //sockfd可写不一定建立了连接，这里通过此再次判断一下
    if (err)
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd);
    }
    else if (sockets::isSelfConnect(sockfd)) //判断是否是自连接（源端IP/PORT=目的端IP/PORT），原因见书籍P328
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd);
    }
    else
    {
      setState(kConnected);  //设置状态为已经连接
      if (connect_)
      {
        newConnectionCallback_(sockfd);
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError state=" << state_;
  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
    retry(sockfd);
  }
}

void Connector::retry(int sockfd) //重新尝试连接
{
  sockets::close(sockfd); //关闭原有的sockfd。每次尝试连接，都需要使用新sockfd
  setState(kDisconnected);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";
    //隔一段时间后重连，重新启用startInLoop
    loop_->runAfter(retryDelayMs_/1000.0,
                    boost::bind(&Connector::startInLoop, shared_from_this()));
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);  //重连延迟加倍，但不超过最大延迟
  }
  else   //超出最大重连时间后，输出连接失败
  {
    LOG_DEBUG << "do not connect";
  }
}

