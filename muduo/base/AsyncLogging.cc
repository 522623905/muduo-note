#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval), //设置超时时间
    running_(false),
    basename_(basename), //文件名
    rollSize_(rollSize), //文件滚动大小
    thread_(boost::bind(&AsyncLogging::threadFunc, this), "Logging"), //日志线程,设置线程启动执行的函数
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer), //当前缓冲区
    nextBuffer_(new Buffer),  //预备缓冲区
    buffers_() //待写入文件的已填满的缓冲列表
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16); //缓冲区列表预留16个空间
}

//前端写日志消息进currentBuffer_缓冲并在写满时通知后端
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)  
  {
    currentBuffer_->append(logline, len); //当前currentBuffer_未满，直接追加日志数据到当前Buffer
  }
  else  
  {
    buffers_.push_back(currentBuffer_.release()); //否则，说明当前缓冲已写满，就先把currentBuffer_记录的日志消息移入待写入文件的buffers_列表(ptr_vector::release()函数把指针从容器中删除,并返回这个指针)

    if (nextBuffer_) //如果nextBuffer_存在
    {
      currentBuffer_ = boost::ptr_container::move(nextBuffer_); //把预备好的另一块缓冲nextBuffer_移用为当前缓冲
    }
    else //如果nextBuffer_不存在
    {
      currentBuffer_.reset(new Buffer); // Rarely happens  那就再分配一块新的Buffer(如果前端写入太快，导致current和next Buffer缓冲都用完了)
    }
    currentBuffer_->append(logline, len); //currentBuffer_弄好后，追加日志消息到currentBuffer_中
    cond_.notify(); //并通知（唤醒）后端开始写入日志数据
  }
}

//接收方后端线程把前端传来的日志写入到文件中
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false); //创建一个把日志记录到文件的对象
  BufferPtr newBuffer1(new Buffer); //先准备好两块空闲的buffer
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite; //要写入到文件的Buffer列表
  buffersToWrite.reserve(16); //列表预留16个空间
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage! buffers_由前端append()负责写入
      {
        cond_.waitForSeconds(flushInterval_); //在临界区内等待条件触发。1.超时（flushInterval_） 2.前端写满了一个或多个buffer，见append
      }
      buffers_.push_back(currentBuffer_.release()); //当条件满足，先将当前缓冲移入buffers
      currentBuffer_ = boost::ptr_container::move(newBuffer1);  //并立刻将空闲的newBuffer移为当前缓冲
      buffersToWrite.swap(buffers_);  //把要写入文件的日志消息列表buffer_到buffersToWrite
      if (!nextBuffer_)
      {
        nextBuffer_ = boost::ptr_container::move(newBuffer2); //交换buffer，使前端能一直有一个预备的buffer可用
      }
    }

    assert(!buffersToWrite.empty());

    //消息堆积
    //前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这就是典型的生产速度
    //超过消费速度的问题，会造成数据在内存中堆积，严重时引发性能问题（可用内存不足）
    //或程序崩溃（分配内存失败）
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf))); //记录上面信息
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());//仅保留前两块缓冲区的日志消息，丢掉多余日志，以腾出内存
    }

    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i].data(), buffersToWrite[i].length());
    }

    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);//仅保留两个buffer，用于newBuffer1 newBuffer2
    }

    //重置newBuffer1
    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    //重置newBuffer2
    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear(); //清空所有
    output.flush();//刷新流
  }
  output.flush(); //线程结束前，刷新流
}

