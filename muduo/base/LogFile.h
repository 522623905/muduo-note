#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

namespace FileUtil
{
class AppendFile;
}

//用于把日志记录到文件的类
//一个典型的日志文件名：logfile_test.20130411-115604.popo.7743.log
//                  运行程序.时间.主机名.线程名.log
class LogFile : boost::noncopyable
{
 public:
  LogFile(const string& basename, //最后/符号后的字符串
          size_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3,
          int checkEveryN = 1024);//默认分割行数1024
  ~LogFile();

  void append(const char* logline, int len);//将一行长度为len添加到日志文件中
  void flush();//刷新
  bool rollFile();//滚动文件

 private:
  void append_unlocked(const char* logline, int len);//不加锁的append方式

  static string getLogFileName(const string& basename, time_t* now);//生成日志文件的名称(运行程序.时间.主机名.线程名.log)

  const string basename_; //日志文件名前面部分
  const size_t rollSize_; // 日志文件达到rollSize_换一个新文件
  const int flushInterval_; // 日志写入文件超时间隔时间
  const int checkEveryN_;

  int count_; //计数行数，检测是否需要换新文件（count_>checkEveryN_也会滚动文件）

  boost::scoped_ptr<MutexLock> mutex_; //加锁
  time_t startOfPeriod_; // 开始记录日志时间（调整到零时时间, 时间/ kRollPerSeconds_ * kRollPerSeconds_）
  time_t lastRoll_; // 上一次滚动日志文件时间
  time_t lastFlush_; // 上一次日志写入文件时间
  boost::scoped_ptr<FileUtil::AppendFile> file_; //文件智能指针

  const static int kRollPerSeconds_ = 60*60*24; //一天的时间
};

}
#endif  // MUDUO_BASE_LOGFILE_H
