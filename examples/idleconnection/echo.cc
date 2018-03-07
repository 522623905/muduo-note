#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include <assert.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int idleSeconds)
  : server_(loop, listenAddr, "EchoServer"),
    connectionBuckets_(idleSeconds)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));
  connectionBuckets_.resize(idleSeconds);
  dumpConnectionBuckets();
}

void EchoServer::start()
{
  server_.start();
}

void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
    EntryPtr entry(new Entry(conn));   //让强引用计数+1
    connectionBuckets_.back().insert(entry);  //将该强引用指针放入Bucket的末尾的set中（即放到timewheel末尾）
    dumpConnectionBuckets();
    WeakEntryPtr weakEntry(entry);  //获取弱指针
    conn->setContext(weakEntry);//将弱指针保存在conn中,如果将强指针存于conn 中则永远删不掉的,因为引用计数永远大于0了(这里的目的是保存弱指针，因为后面收到数据时还要用到Entry)
  }
  else
  {
    assert(!conn->getContext().empty());
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));
    LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
  }
}

void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString()); //以字符串形式获取客户端发送信息
  LOG_INFO << conn->name() << " echo " << msg.size()
           << " bytes at " << time.toString();
  conn->send(msg);  //反馈回去客户端

  assert(!conn->getContext().empty());
  WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));  //把在connection回调中存储的上下文（boost:any类型）转换出来WeakEntryPtr类型
  EntryPtr entry(weakEntry.lock()); //获取强引用指针
  if (entry)
  {
    connectionBuckets_.back().insert(entry);  //因为连接收到了数据，所以在时间轮尾端插入entry，相当于增加寿命
    dumpConnectionBuckets();
  }
}

void EchoServer::onTimer()  //每秒的回调函数，模拟时间轮
{
  connectionBuckets_.push_back(Bucket()); //往队尾添加一个空的Bucket，这样circular_buffer会自动弹出队首的Bucket，并析构之
  dumpConnectionBuckets();
}

void EchoServer::dumpConnectionBuckets() const      //打印circular_buffer的变化情况
{
  LOG_INFO << "size = " << connectionBuckets_.size(); //大小即为idleSeconds，在构造函数指定
  int idx = 0;
  for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.begin();   //依次扫描circular_buffer的元素bucket
      bucketI != connectionBuckets_.end();
      ++bucketI, ++idx)
  {
    const Bucket& bucket = *bucketI;
    printf("[%d] len = %zd : ", idx, bucket.size());  //每个bucket的大小，即为存储着过相同时间就被关闭的链接的个数
    for (Bucket::const_iterator it = bucket.begin();
        it != bucket.end();
        ++it)
    {
      bool connectionDead = (*it)->weakConn_.expired(); //expired用于判断weak_ptr指向的对象是否已被销毁(资源的引用计数是否为0)
      printf("%p(%ld)%s, ", get_pointer(*it), it->use_count(),      //代表地址、引用计数
          connectionDead ? " DEAD" : "");   
    }
    puts("");
  }
}

