// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include <muduo/base/CurrentThread.h>
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG
__BEGIN_DECLS
extern void __assert_perror_fail (int errnum,
                                  const char *file,
                                  unsigned int line,
                                  const char *function)
    __THROW __attribute__ ((__noreturn__));
__END_DECLS
#endif

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       if (__builtin_expect(errnum != 0, 0))    \
                         __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

#else  // CHECK_PTHREAD_RETURN_VALUE

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum;})

#endif // CHECK_PTHREAD_RETURN_VALUE

namespace muduo
{

// Use as data member of a class, eg.
//
// class Foo
// {
//  public:
//   int size() const;
//
//  private:
//   mutable MutexLock mutex_;
//   std::vector<int> data_; // GUARDED BY mutex_
// };
//互斥器(不可拷贝)
class MutexLock : boost::noncopyable
{
 public:
  MutexLock()
    : holder_(0)
  {
    MCHECK(pthread_mutex_init(&mutex_, NULL));//构造函数中初始化mutex
  }

  ~MutexLock()
  {
    assert(holder_ == 0);//断言锁没有被任何线程使用
    MCHECK(pthread_mutex_destroy(&mutex_));//析构函数中销毁mutex
  }

  // must be called when locked, i.e. for assertion
  bool isLockedByThisThread() const     //用来检查当前线程是否给这个MutexLock对象加锁
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const
  {
    assert(isLockedByThisThread());
  }

  // internal usage
    //上锁
  void lock()
  {
    MCHECK(pthread_mutex_lock(&mutex_));  
    assignHolder();//记录加锁的线程tid
  }

    //解锁
  void unlock()
  {
    unassignHolder();//记住清零holder
    MCHECK(pthread_mutex_unlock(&mutex_));
  }

//可以返回指向类对象中互斥量的指针，在类外对互斥量操作，这个主要用在条件变量中
  pthread_mutex_t* getPthreadMutex() /* non-const */ 
  {
    return &mutex_;
  }

 private:
  friend class Condition; //友元Condition,为的是其能操作MutexLock类

  class UnassignGuard : boost::noncopyable
  {
   public:
    UnassignGuard(MutexLock& owner)
      : owner_(owner)
    {
      owner_.unassignHolder();    //构造函数给holder_置零
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();  //析构函数给holder_赋值线程tid
    }

   private:
    MutexLock& owner_;
  };

  void unassignHolder()   //解锁时给holder_置零(在解锁前调用)
  {
    holder_ = 0;
  }

  void assignHolder()     //上锁时记录线程tid给holder_赋值(在上锁后调用)
  {
    holder_ = CurrentThread::tid();   //当前线程的tid
  }

  pthread_mutex_t mutex_;   //互斥量
  pid_t holder_;  //用来表示给互斥量上锁线程的tid
};

// Use as a stack variable, eg.
// int Foo::size() const
// {
//   MutexLockGuard lock(mutex_);
//   return data_.size();
// }
//在使用mutex时，有时会忘记给mutex解锁，为了防止这种情况发生，常常使用RAII手法
class MutexLockGuard : boost::noncopyable
{
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();//在构造函数初始化，并上锁
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();//在析构函数解锁
  }

 private:

  MutexLock& mutex_;    //变量的引用
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
