// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Exception.h>
#include <muduo/base/Logging.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/weak_ptr.hpp>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace CurrentThread
{
//__thread修饰的变量是线程局部存储的，否则是多个线程共享
  __thread int t_cachedTid = 0;//线程真实pid(tid)的缓存，减少::syscall(SYS_gettid)调用次数
  __thread char t_tidString[32];//tid的字符串表示形式
  __thread int t_tidStringLength = 6;//上述字符串的长度
  __thread const char* t_threadName = "unknown";//线程名称
  const bool sameType = boost::is_same<int, pid_t>::value; //用于判断两个类型是否一样
  BOOST_STATIC_ASSERT(sameType);
}

namespace detail
{

//通过系统调用获取tid
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

//fork之后的子进程要执行的函数
void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid(); //缓存线程tid
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

//主线程名的初始化
class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main"; //设置主线程名main
    CurrentThread::tid(); //缓存main线程tid
    //pthread_atfork()在fork()之前调用，当调用fork时，内部创建子进程前在父进程中会调用prepare
    //内部创建子进程成功后，父进程会调用parent ，子进程会调用child
    pthread_atfork(NULL, NULL, &afterFork);//若在进程中调用fork()函数，则子进程会调用afterFork()函数
  }
};

ThreadNameInitializer init; //全局变量，在命名空间中，引用库的时候初始化，因此在main函数执行前则会初始化了！

//线程数据类，观察者模式，
//通过回调函数传递给子线程的数据
struct ThreadData
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;//线程函数
  string name_;//线程名
  boost::weak_ptr<pid_t> wkTid_;//线程tid

  ThreadData(const ThreadFunc& func,
             const string& name,
             const boost::shared_ptr<pid_t>& tid)
    : func_(func),
      name_(name),
      wkTid_(tid)
  { }

  //ThreadData具体的执行
  void runInThread()
  {
    pid_t tid = muduo::CurrentThread::tid();//计算本线程id，保存在线程局部变量里

    boost::shared_ptr<pid_t> ptid = wkTid_.lock();//tid弱引用提升为强引用
    if (ptid)//如果提升成功
    {
      *ptid = tid;//成功了之后通过智能指针修改父类线程id，
      ptid.reset();//该临时shared_ptr销毁掉
    }

    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);//为线程指定名字
    try
    {
      func_();//运行线程函数
      muduo::CurrentThread::t_threadName = "finished";//运行结束后的threadname
    }
    //捕获线程函数是否有异常
    catch (const Exception& ex)//Exception异常
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
      abort();
    }
    catch (const std::exception& ex) //标准异常
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...) //其他异常
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow  再次抛出异常
    }
  }
};

//父线程交给系统的回调，obj是父线程传递的threadData
void* startThread(void* obj)
{
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread(); //线程启动
  delete data;
  return NULL;
}

}
}

using namespace muduo;

//缓存tid到t_cachedTid
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid();//通过系统调用获取唯一的tid
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);//返回字符串长度
  }
}

//判断是不是主线程
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

//休眠
void CurrentThread::sleepUsec(int64_t usec)
{
  struct timespec ts = { 0, 0 };
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;//因为numCreated_是静态的，一定要定义，不能只在头文件中声明

Thread::Thread(const ThreadFunc& func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(new pid_t(0)),
    func_(func),
    name_(n)
{
  setDefaultName();
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
Thread::Thread(ThreadFunc&& func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(new pid_t(0)),
    func_(std::move(func)),
    name_(n)
{
  setDefaultName();//设置线程默认名字为Thread+序号
}

#endif

Thread::~Thread()
{
  if (started_ && !joined_)
  {
    pthread_detach(pthreadId_); //析构时，如果没有调用join,则与线程分离
  }
}

//设置线程默认名字为Thread+序号
void Thread::setDefaultName()
{
  int num = numCreated_.incrementAndGet(); //创建的线程个数+1
  if (name_.empty())
  {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}

//启动线程
void Thread::start()
{
  assert(!started_);
  started_ = true;
  // FIXME: move(func_)
  detail::ThreadData* data = new detail::ThreadData(func_, name_, tid_);
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) //创建线程并启动
  {
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
}

int Thread::join()  
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, NULL);  //阻塞等待pthreadId_线程结束,使得Thread对象的生命期长于线程
}                             //否则，若Thread对象的生命期短于线程，那么析构时会自动detach线程，造成资源泄露

