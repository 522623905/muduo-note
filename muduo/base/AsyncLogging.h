#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include <muduo/base/BlockingQueue.h>
#include <muduo/base/BoundedBlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/LogStream.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{


/*
  muduo异步日志库采用的双缓冲技术：
  前端负责往Buffer A填日志消息，后端负责将Buffer B的日志消息写入文件。
  当Buffer A写满后，交换A和B，让后端将Buffer A的数据写入文件，而前端则往Buffer B
  填入新的日志消息，如此往复。
*/

class AsyncLogging : boost::noncopyable
{
 public:

  AsyncLogging(const string& basename, //文件名
               size_t rollSize,     //滚动大小到一定值，则换一个文件
               int flushInterval = 3); //超时时间默认3s（在超时时间内没有写满，也要将缓冲区的数据添加到文件当中）

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  //供前端生产者线程调用（日志数据写到缓冲区）
  void append(const char* logline, int len);

  void start()
  {
    running_ = true;
    thread_.start(); //日志线程启动
    latch_.wait(); //等待线程已经启动，才继续往下执行
  }

  void stop()
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  // declare but not define, prevent compiler-synthesized functions
  AsyncLogging(const AsyncLogging&);  // ptr_container
  void operator=(const AsyncLogging&);  // ptr_container

  void threadFunc();//供后端消费者线程调用（将数据写到日志文件）

  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer; //固定一个Buffer大小为4M
  typedef boost::ptr_vector<Buffer> BufferVector; //ptr_vector自动管理动态内存的生命期
  typedef BufferVector::auto_type BufferPtr;  //Buffer列表。auto_type类似std::unique_ptr，具备移动语义（所有权只有一个），能自动管理对象生命期

  const int flushInterval_; //超时时间，在超时时间内没有写满，也要将这块缓冲区的数据添加到文件当中
  bool running_; //线程是否执行的标志
  string basename_; //日志文件名称
  size_t rollSize_; //日志文件滚动大小，当超过一定大小，则滚动一个新的日志文件
  muduo::Thread thread_; //使用了一个单独的线程来记录日志！
  muduo::CountDownLatch latch_; //用于等待线程启动
  muduo::MutexLock mutex_;
  muduo::Condition cond_;
  BufferPtr currentBuffer_; //当前缓冲
  BufferPtr nextBuffer_;  //预备缓冲
  BufferVector buffers_;  //待写入文件的已填满的缓冲列表，供后端写入的buffer
};

}
#endif  // MUDUO_BASE_ASYNCLOGGING_H

