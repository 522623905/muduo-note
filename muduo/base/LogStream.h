#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <assert.h>
#include <string.h> // memcpy
#ifndef MUDUO_STD_STRING
#include <string>
#endif
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;  //4k
const int kLargeBuffer = 4000*1000; //4M

//一个固定大小SIZE的Buffer类
template<int SIZE>
class FixedBuffer : boost::noncopyable
{
 public:
  FixedBuffer()
    : cur_(data_)
  {
    setCookie(cookieStart);//设置cookie，muduo库这个函数目前还没加入功能，所以可以不用管
  }

  ~FixedBuffer()
  {
    setCookie(cookieEnd);
  }

  //如果可用数据足够，就拷贝buf过去，同时移动当前指针
  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }

  const char* data() const { return data_; }//返回首地址
  int length() const { return static_cast<int>(cur_ - data_); }//返回缓冲区已有数据长度

  // write to data_ directly
  char* current() { return cur_; }//返回当前数据末端地址
  int avail() const { return static_cast<int>(end() - cur_); }//返回剩余可用地址
  void add(size_t len) { cur_ += len; }//cur前移

  void reset() { cur_ = data_; }//重置，不清数据，只需要让cur指回首地址即可
  void bzero() { ::bzero(data_, sizeof data_); }//清零

  // for used by GDB
  const char* debugString();
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string toString() const { return string(data_, length()); }//返回string类型
  StringPiece toStringPiece() const { return StringPiece(data_, length()); }

 private:
  const char* end() const { return data_ + sizeof data_; }//返回尾指针
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();
  char data_[SIZE]; //缓冲区数据，大小为size
  char* cur_; //当前指针,永远指向已有数据的最右端
};

}

//重载了各种<<,负责把各个类型的数据转换成字符串，
//再添加到FixedBuffer中
//该类主要负责将要记录的日志内容放到这个Buffer里面
class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  //把下面各个类型转换为字符串后存储到buffer中
  //(为何不用stringstream类来转换，这个更方便？)
  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v)
  {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* str)
  {
    if (str)
    {
      buffer_.append(str, strlen(str));
    }
    else
    {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  self& operator<<(const unsigned char* str)
  {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

#ifndef MUDUO_STD_STRING
  self& operator<<(const std::string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }
#endif

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v)
  {
    *this << v.toStringPiece();
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);//把int类型的value转换为字符串类型,添加到buffer_中

  Buffer buffer_; //固定大小的FixedBuffer

  static const int kMaxNumericSize = 32;
};

//使用snprintf来格式化（format）成字符串
class Fmt // : boost::noncopyable
{
 public:
  template<typename T>
  Fmt(const char* fmt, T val);//按照fmt的格式来格式val成字符串buf

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];//存储格式化后组成的字符串
  int length_;
};

inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

}
#endif  // MUDUO_BASE_LOGSTREAM_H

