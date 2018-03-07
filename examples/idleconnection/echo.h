#ifndef MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
#define MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H

#include <muduo/net/TcpServer.h>
//#include <muduo/base/Types.h>

#include <boost/circular_buffer.hpp>
#include <boost/unordered_set.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < 104700
namespace boost
{
template <typename T>
inline size_t hash_value(const boost::shared_ptr<T>& x)
{
  return boost::hash_value(x.get());  //它接受唯一的一个参数来指明需要计算 Hash 值的对象的类型.
}                                                       //每当一个对象需要计算它的 Hash 值时， hash_value() 都会自动被调用          
                                                         //get函数返回内部对象(指针)
}
#endif

// RFC 862
class EchoServer
{
 public:
  EchoServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr,
             int idleSeconds);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  void onTimer();

  void dumpConnectionBuckets() const;

  typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;

  struct Entry : public muduo::copyable
  {
    explicit Entry(const WeakTcpConnectionPtr& weakConn)  /*初始化时候获得muduo::net::TcpConnection类型对象的弱指针*/
      : weakConn_(weakConn)
    {
    }

    ~Entry()     /*析构时候将弱指针提升为强指针,提升成功则关闭写端*/
    {
      muduo::net::TcpConnectionPtr conn = weakConn_.lock(); //weak_ptr可以使用一个非常重要的成员函数lock()从被观测的shared_ptr获得一个可用的shared_ptr对象， 从而操作资源
      if (conn)
      {
        conn->shutdown();   //析构函数判断链接存在则断开
      }
    }

    WeakTcpConnectionPtr weakConn_; 
  };
  typedef boost::shared_ptr<Entry> EntryPtr;
  typedef boost::weak_ptr<Entry> WeakEntryPtr;  //弱引用，weak_ptr不会增加shared_ptr对象的引用计数
  typedef boost::unordered_set<EntryPtr> Bucket;  //unordered_set是散列容器，基于哈希表实现，通过相应哈希函数处理关键字得到相应关键值
  typedef boost::circular_buffer<Bucket> WeakConnectionList;  //circular_buffer实现了循环缓冲的数据结构，但大小固定，当到达容器末尾，将自动循环利用容器的另一端空间(如满时，在末尾添加，则begin处元素被删除，begin+1处成为新的begin，依次。。；如果在头添加则反过来)

  muduo::net::TcpServer server_;
  WeakConnectionList connectionBuckets_;  //存放连接的强指针
};

#endif  // MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
