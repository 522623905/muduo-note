#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

class LengthHeaderCodec : boost::noncopyable
{
 public:
  typedef boost::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)   //构造函数则是设置messageCallback_回调函数
    : messageCallback_(cb)
  {
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn,      
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4   遵守了协议规定，前4个字节表示消息长度
    {
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->peek();   //peek返回的是buf中可读消息的位置
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS    消息长度
      const int32_t len = muduo::net::sockets::networkToHost32(be32);   //长度的网络序转换成主机序
      if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown();  // FIXME: disable reading
        break;
      }
      else if (buf->readableBytes() >= len + kHeaderLen)
      {
        buf->retrieve(kHeaderLen);  //移动readIndex位置，表示不要消息长度的信息
        muduo::string message(buf->peek(), len);    //获取len长度的消息
        messageCallback_(conn, message, receiveTime); //回调函数，此处回调的是ChatServer::onStringMessage，把消息发送出去
        buf->retrieve(len); //移动readIndex位置
      }
      else
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection* conn,      //打包把muduo::string转换成muduo::Buffer的发送函数
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;
    buf.append(message.data(), message.size());   //获取message消息加到buf中
    int32_t len = static_cast<int32_t>(message.size());   //消息的长度
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len); //把消息长度，主机序转换为网络字节序
    buf.prepend(&be32, sizeof be32);  //把消息长度加到buf前缀
    conn->send(&buf);   //把muduo::string封装成muduo::Buffer后发送
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
