#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <map>
#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

namespace pubsub
{

typedef std::set<string> ConnectionSubscription;  //主要存储每个客户conn订阅的topic主题名


//Topic类，该实例包含有订阅的客户信息和topic内容
class Topic : public muduo::copyable
{
 public:
  Topic(const string& topic)
    : topic_(topic)  //话题名
  {
  }

  void add(const TcpConnectionPtr& conn)  //给topic添加一个订阅客户
  {
    audiences_.insert(conn);
    if (lastPubTime_.valid())
    {
      conn->send(makeMessage());
    }
  }

  void remove(const TcpConnectionPtr& conn)  //移除该订阅客户
  {
    audiences_.erase(conn);
  }

  void publish(const string& content, Timestamp time) //发表内容至订阅的客户
  {
    content_ = content;
    lastPubTime_ = time;
    string message = makeMessage();
    for (std::set<TcpConnectionPtr>::iterator it = audiences_.begin();
         it != audiences_.end();
         ++it)
    {
      (*it)->send(message);   //依次发送给订阅的客户
    }
  }

 private:

  string makeMessage()
  {
    return "pub " + topic_ + "\r\n" + content_ + "\r\n"; //按格式制作一条消息
  }

  string topic_;  //话题
  string content_;  //内容
  Timestamp lastPubTime_; //上次提交时间
  std::set<TcpConnectionPtr> audiences_;  //所有订阅的客户
};

//服务器类
class PubSubServer : boost::noncopyable
{
 public:
  PubSubServer(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "PubSubServer")
  {
    server_.setConnectionCallback(
        boost::bind(&PubSubServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&PubSubServer::onMessage, this, _1, _2, _3));
    loop_->runEvery(1.0, boost::bind(&PubSubServer::timePublish, this));  //每秒发送一次给订阅utc_time的客户时间信息
  }

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      conn->setContext(ConnectionSubscription()); //给连接的客户端保存一个空std::set<string>,在doSubscribe()中用到
    }
    else  //若客户连接断开，
    {
      const ConnectionSubscription& connSub
        = boost::any_cast<const ConnectionSubscription&>(conn->getContext()); //取出保存的上下文，即订阅的topic名
      // subtle: doUnsubscribe will erase *it, so increase before calling.
      for (ConnectionSubscription::const_iterator it = connSub.begin();
           it != connSub.end();)
      {
        doUnsubscribe(conn, *it++);   //取消客户端conn的订阅的topic
      }
    }
  }

//接受消息回调，作为中继转发作用
  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp receiveTime)
  {
    ParseResult result = kSuccess;
    while (result == kSuccess)
    {
      string cmd;
      string topic;
      string content;
      result = parseMessage(buf, &cmd, &topic, &content); //从buf中解析出 cmd topic content
      if (result == kSuccess)
      {
        if (cmd == "pub")  //若为pub，则转发内容给订阅topic的客户
        {
          doPublish(conn->name(), topic, content, receiveTime); 
        }
        else if (cmd == "sub")  //若为sub，则登记要订阅topic的客户信息
        {
          LOG_INFO << conn->name() << " subscribes " << topic;
          doSubscribe(conn, topic);  
        }
        else if (cmd == "unsub")  //若为unsub，则对conn客户取消订阅topic内容
        {
          doUnsubscribe(conn, topic); 
        }
        else  //若cmd错误，则关闭连接
        {
          conn->shutdown(); 
          result = kError;
        }
      }
      else if (result == kError)
      {
        conn->shutdown();  //消息错误，则关闭连接
      }
    }
  }

  //订阅utc_time的客户会收到的信息
  void timePublish()
  {
    Timestamp now = Timestamp::now();
    doPublish("internal", "utc_time", now.toFormattedString(), now);
  }

  //登记要订阅topic的客户信息
  void doSubscribe(const TcpConnectionPtr& conn,
                   const string& topic)
  {
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());

    connSub->insert(topic); //插入topic到保存的上下文当中
    getTopic(topic).add(conn); //获取topic对应的Topic类，在topics_中添加客户conn,表示conn订阅topic信息
  }

  //取消客户conn订阅的topic
  void doUnsubscribe(const TcpConnectionPtr& conn,
                     const string& topic)
  {
    LOG_INFO << conn->name() << " unsubscribes " << topic;
    getTopic(topic).remove(conn);  //从topics_中的Topic类移除conn
    // topic could be the one to be destroyed, so don't use it after erasing.
    ConnectionSubscription* connSub
      = boost::any_cast<ConnectionSubscription>(conn->getMutableContext());
    connSub->erase(topic);
  }

  //发布topic内容给订阅topic的客户
  void doPublish(const string& source,
                 const string& topic,
                 const string& content,
                 Timestamp time)
  {
    getTopic(topic).publish(content, time);
  }

  //获取topic对应的Topic类实例引用，该实例包含有客户信息和topic内容
  Topic& getTopic(const string& topic)
  {
    std::map<string, Topic>::iterator it = topics_.find(topic); //在topics_中查找有没有topic
    if (it == topics_.end()) //若topics_中无topic，则插入,否则直接返还topic对应的实例
    {
      it = topics_.insert(make_pair(topic, Topic(topic))).first;  
    }
    return it->second;  //返回Topic实例
  }

  EventLoop* loop_;
  TcpServer server_;
  std::map<string, Topic> topics_; //一个topic对应一个Topic结构，所有的保存至topics_中
};

}

int main(int argc, char* argv[])
{
  if (argc > 1)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    EventLoop loop;
    if (argc > 2)
    {
      //int inspectPort = atoi(argv[2]);
    }
    pubsub::PubSubServer server(&loop, InetAddress(port));
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s pubsub_port [inspect_port]\n", argv[0]);
  }
}

