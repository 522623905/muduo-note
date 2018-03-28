// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <assert.h>
#include <stdlib.h> // atexit
#include <pthread.h>

namespace muduo
{

namespace detail
{
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
template<typename T>
struct has_no_destroy
{
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  template <typename C> static char test(decltype(&C::no_destroy));
#else
  template <typename C> static char test(typeof(&C::no_destroy));
#endif
  template <typename C> static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};
}

/*
单例模式需要：
  1.私有构造函数  
  2.一个静态方法，返回这个唯一实例的引用
  3.一个指针静态变量
  4.选择一个解决多线程问题的方法
*/
//线程安全的singleton
template<typename T>
class Singleton : boost::noncopyable
{
 public:
  static T& instance()  //返回static的唯一实例的引用
  {
    pthread_once(&ponce_, &Singleton::init);  //使用初值为PTHREAD_ONCE_INIT的变量保证init函数在本进程执行序列中只执行一次
    assert(value_ != NULL);
    return *value_;
  }

 private:
  Singleton();  //私有构造函数，使得外部不能创建临时对象
  ~Singleton(); //私有析构函数，保证只能在堆上new一个新的类对象

  //在内部创建的对象
  static void init()  
  {
    value_ = new T();
    if (!detail::has_no_destroy<T>::value)
    {
      ::atexit(destroy);//登记销毁函数，在整个程序结束时会自动调用该函数来销毁对象
    }
  }

  //在内部销毁的对象
  static void destroy()
  {
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//使不完全类型的指针在编译器报错，而不是警告
    T_must_be_complete_type dummy; (void) dummy;

    delete value_;
    value_ = NULL;
  }

 private:
  static pthread_once_t ponce_; //这个对象保证函数只被执行一次
  static T*             value_; //指针静态变量
};

//静态变量均需要初始化
template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}
#endif

