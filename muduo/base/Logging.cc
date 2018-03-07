#include <muduo/base/Logging.h>

#include <muduo/base/CurrentThread.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/TimeZone.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace muduo
{

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

__thread char t_errnobuf[512];
__thread char t_time[32];
__thread time_t t_lastSecond;

const char* strerror_tl(int savedErrno)
{
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

//初始化日志级别
Logger::LogLevel initLogLevel()
{
  if (::getenv("MUDUO_LOG_TRACE"))
    return Logger::TRACE;
  else if (::getenv("MUDUO_LOG_DEBUG"))
    return Logger::DEBUG;
  else
    return Logger::INFO;
}

Logger::LogLevel g_logLevel = initLogLevel();

const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

// helper class for known string length at compile time
 //编译时获取字符串长度的类
class T
{
 public:
  T(const char* str, unsigned len)
    :str_(str),
     len_(len)
  {
    assert(strlen(str) == len_);
  }

  const char* str_;
  const unsigned len_;
};

inline LogStream& operator<<(LogStream& s, T v)
{
  s.append(v.str_, v.len_);
  return s;
}

inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
{
  s.append(v.data_, v.size_);
  return s;
}

//默认输出内容到stdout
void defaultOutput(const char* msg, int len)
{
  size_t n = fwrite(msg, 1, len, stdout);
  //FIXME check n
  (void)n;
}

//默认刷新stdout缓冲
void defaultFlush()
{
  fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;//默认输出到屏幕
Logger::FlushFunc g_flush = defaultFlush; //默认刷新方法
TimeZone g_logTimeZone;

}

using namespace muduo;

//如一条日志消息：20180115 06:39:01.712150Z  7070 INFO  pid = 7070 - main.cc:13
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
  : time_(Timestamp::now()), //当前时间戳
    stream_(),
    level_(level),
    line_(line),
    basename_(file)
{
  formatTime();//格式化time_时间戳为年月日后存于stream_中
  CurrentThread::tid();//缓存当前线程id
  stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength());//格式化线程tid字符串,先输出到stream_
  stream_ << T(LogLevelName[level], 6);//格式化级别，对应成字符串，先输出到stream_
  if (savedErrno != 0)
  {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";//如果错误码不为0，还要输出相对应信息
  }
}

//格式化time_时间戳为年月日后存于stream_中
void Logger::Impl::formatTime()
{
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch(); //获取us的时间戳
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond); //能转换为的秒时间戳
  int microseconds = static_cast<int>(microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond); //剩余的us时间戳
  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
    if (g_logTimeZone.valid())
    {
      tm_time = g_logTimeZone.toLocalTime(seconds);
    }
    else
    {
      ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime
    }

    int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec); //格式化时间，年月日时分秒
    assert(len == 17); (void)len;
  }

  if (g_logTimeZone.valid())
  {
    Fmt us(".%06d ", microseconds);//格式化
    assert(us.length() == 8);
    stream_ << T(t_time, 17) << T(us.data(), 8); //用stream进行输出，重载了<<
  }
  else
  {
    Fmt us(".%06dZ ", microseconds);
    assert(us.length() == 9);
    stream_ << T(t_time, 17) << T(us.data(), 9);
  }
}

//将文件名以及代码行号输进缓冲区
void Logger::Impl::finish()
{
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
  : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
  : impl_(level, 0, file, line)
{
  impl_.stream_ << func << ' ';//格式化函数名称，上面的构造函数没有函数名称，不同的构造函数
}

Logger::Logger(SourceFile file, int line, LogLevel level)
  : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, file, line)
{
}

//析构函数中会调用impl_的finish方法
Logger::~Logger()
{
  impl_.finish();//将名字行数输入缓冲区
  const LogStream::Buffer& buf(stream().buffer());//将缓冲区以引用方式获得
  g_output(buf.data(), buf.length());//调用全部输出方法，输出缓冲区内容，默认是输出到stdout
  if (impl_.level_ == FATAL)
  {
    g_flush();
    abort(); //停止程序
  }
}

//设置日志级别
void Logger::setLogLevel(Logger::LogLevel level)
{
  g_logLevel = level;
}

//设置输出函数，用来替代默认的
void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}

//用来配套你设置的输出函数的刷新方法
void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}

void Logger::setTimeZone(const TimeZone& tz)
{
  g_logTimeZone = tz;
}
