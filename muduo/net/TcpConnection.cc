// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpConnection.h>

#include <muduo/base/Logging.h>
#include <muduo/base/WeakCallback.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
  // do not call conn->forceClose(), because some users want to register message callback only.
}

void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
  buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,  // 建立连接需要一个Reactor
                             const string& nameArg,  // 连接名称
                             int sockfd,            // 已经由Acceptor建立好连接的sockfd
                             const InetAddress& localAddr,  // 本地地址
                             const InetAddress& peerAddr)  // 对端地址
  : loop_(CHECK_NOTNULL(loop)),
    name_(nameArg),
    state_(kConnecting),    //因为sockfd是已经建立好连接的sockfd，所以设置kConnecting
    reading_(true),
    socket_(new Socket(sockfd)), //RAII管理套接字
    channel_(new Channel(loop, sockfd)),  //channel与sockfd绑定
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024)  //高水位,64M
{
    //设置channel各种回调
  channel_->setReadCallback(          
      boost::bind(&TcpConnection::handleRead, this, _1));
  channel_->setWriteCallback(
      boost::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(
      boost::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(
      boost::bind(&TcpConnection::handleError, this));
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd()
            << " state=" << stateToString();
  assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
  return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString() const
{
  char buf[1024];
  buf[0] = '\0';
  socket_->getTcpInfoString(buf, sizeof buf);
  return buf;
}

//使得任何结构的data数据转换为StringPiece后均可send
void TcpConnection::send(const void* data, int len)
{
  send(StringPiece(static_cast<const char*>(data), len));
}

void TcpConnection::send(const StringPiece& message)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())  //如果在IO线程中，则直接调用
    {
      sendInLoop(message);
    }
    else
    {   //否则先加入队列，等待唤醒IO线程再调用发送
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,     // FIXME
                      message.as_string()));
                    //std::forward<string>(message)));
    }
  }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer* buf)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());
      buf->retrieveAll();
    }
    else
    {
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,     // FIXME
                      buf->retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
  sendInLoop(message.data(), message.size());
}

//只能在当前IO线程中send
void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0; //表示已被发送至sockfd的数据
  size_t remaining = len;  //未写入的字节数,初始为len
  bool faultError = false;
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) //Buffer中没数据
  {
    nwrote = sockets::write(channel_->fd(), data, len); //直接写入data到socket
    if (nwrote >= 0)
    {
      remaining = len - nwrote; //调整剩下的字节数
      if (remaining == 0 && writeCompleteCallback_) //表示缓冲数据已经发送完成了
      {
        loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));  //调用写完成回调
      }
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK)
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
        {
          faultError = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!faultError && remaining > 0)  //如果数据没有一次性发送完毕
  {
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_   
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)  //缓冲数据过多，调用高水位回调函数
    {
      loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining); //将剩下的remaining个数据放入缓冲outputBuffer_
    if (!channel_->isWriting())
    {
      channel_->enableWriting(); //Buffer中有数据，开始writeable事件.poll返回会在TcpConnection::handleWrite中把缓冲数据发送出去
    }
  }
}

//半关闭，关闭写端
void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)
  {
    setState(kDisconnecting);   //状态转换:  kConnected  ->  kDisconnecting
    // FIXME: shared_from_this()?
    loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  if (!channel_->isWriting())
  {
    // we are not writing
    socket_->shutdownWrite(); //半关闭，关闭写端
  }
}

// void TcpConnection::shutdownAndForceCloseAfter(double seconds)
// {
//   // FIXME: use compare and swap
//   if (state_ == kConnected)
//   {
//     setState(kDisconnecting);
//     loop_->runInLoop(boost::bind(&TcpConnection::shutdownAndForceCloseInLoop, this, seconds));
//   }
// }

// void TcpConnection::shutdownAndForceCloseInLoop(double seconds)
// {
//   loop_->assertInLoopThread();
//   if (!channel_->isWriting())
//   {
//     // we are not writing
//     socket_->shutdownWrite();
//   }
//   loop_->runAfter(
//       seconds,
//       makeWeakCallback(shared_from_this(),
//                        &TcpConnection::forceCloseInLoop));
// }

//主动关闭Connection
void TcpConnection::forceClose()
{
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->queueInLoop(boost::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

//延迟关闭，使用了定时器
void TcpConnection::forceCloseWithDelay(double seconds)
{
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->runAfter(
        seconds,
        makeWeakCallback(shared_from_this(),
                         &TcpConnection::forceClose));  // not forceCloseInLoop to avoid race condition
  }
}

void TcpConnection::forceCloseInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    // as if we received 0 byte in handleRead();
    handleClose();
  }
}

const char* TcpConnection::stateToString() const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
  loop_->runInLoop(boost::bind(&TcpConnection::startReadInLoop, this));
}

//关注可读事件
void TcpConnection::startReadInLoop()
{
  loop_->assertInLoopThread();
  if (!reading_ || !channel_->isReading())
  {
    channel_->enableReading();
    reading_ = true;
  }
}

void TcpConnection::stopRead()
{
  loop_->runInLoop(boost::bind(&TcpConnection::stopReadInLoop, this));
}

//取消关注读事件
void TcpConnection::stopReadInLoop()
{
  loop_->assertInLoopThread();
  if (reading_ || channel_->isReading())
  {
    channel_->disableReading();
    reading_ = false;
  }
}

//连接建立。在TcpServer中建立连接后会调用此函数,关注读
void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);   //状态转换： kConnecting  ->  kConnected
  channel_->tie(shared_from_this());
  channel_->enableReading();  //开启读事件(加入到Poller关注)

  connectionCallback_(shared_from_this());  //在此处执行服务端注册的onConnection连接回调
}

// 关闭连接，提供给TcpServer使用
// 是TcpConnection析构前最后调用的一个成员函数
void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll(); // 停止监听所有事件

    connectionCallback_(shared_from_this());  //调用到用户定义的的连接回调函数
  }
  channel_->remove(); //将channel_移除,即从epoll中移除fd，该conn在loop中彻底移除
}

//数据到来,调用用户定义的接收信息回调
void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)  //仍有数据可读
  {
      //用户定义的消息到来回调函数在此执行!
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  else if (n == 0)  //非阻塞对端关闭了连接则read 0
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
}

//处理可写事件
void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();
  if (channel_->isWriting())  //如果sockfd可写触发了可写事件
  {
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());  //把Buffer缓冲的数据写入sockfd
    if (n > 0)
    {
      outputBuffer_.retrieve(n);  //移动Buffer位置
      if (outputBuffer_.readableBytes() == 0) //Buffer已经发送完毕了
      {
        channel_->disableWriting(); //先禁用writable事件，否则poll会busy loop
        if (writeCompleteCallback_) //此处进行写完成回调的函数
        {
          loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
        }
        //确保了,如果outputBuffer_没有数据了,才关闭连接
        //避免了本端要发送的数据对端未收到就关闭连接
        if (state_ == kDisconnecting)
        {
          shutdownInLoop();
        }
      }
    }
    else
    {
      LOG_SYSERR << "TcpConnection::handleWrite";
      // if (state_ == kDisconnecting)
      // {
      //   shutdownInLoop();
      // }
    }
  }
  else
  {
    LOG_TRACE << "Connection fd = " << channel_->fd()
              << " is down, no more writing";
  }
}

//处理关闭事件
void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected); //状态转换：  kConnected  ->  kDisconnected
  channel_->disableAll(); //此后不再收发数据

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);//调用用户设置的连接回调
  // must be the last line
  closeCallback_(guardThis);  //实际是调用TcpServer::removeConnection,在TcpServer中设定conn->setCloseCallback
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

