// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <muduo/base/Mutex.h>  // MCHECK

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

//线程局部数据（表面上看起来这是一个全局变量，所有线程都可以使用它，而它的值在每一个线程中又是单独存储的）
template<typename T>
class ThreadLocal : boost::noncopyable
{
 public:
  ThreadLocal()
  {
    MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));//创建key，设置清理函数
  }

  ~ThreadLocal()
  {
    MCHECK(pthread_key_delete(pkey_));//仅仅只是销毁key，实际数据在destructor（）
  }

  T& value()
  {
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));//获取key对应的线程特定数据
    if (!perThreadValue) //为空说明特定数据未创建，则创建
    {
      T* newObj = new T();
      MCHECK(pthread_setspecific(pkey_, newObj));//存储newObj数据（key与数据绑定）
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  //使用该函数来销毁实际数据
  //在线程释放该线程存储的时候被调用
  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    delete obj;
  }

 private:
  pthread_key_t pkey_;//定义key
};

}
#endif
