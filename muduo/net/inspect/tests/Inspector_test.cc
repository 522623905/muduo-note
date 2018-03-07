#include <muduo/net/inspect/Inspector.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

using namespace muduo;
using namespace muduo::net;

int main()
{
  EventLoop loop;
  EventLoopThread t; // 监控线程 ，这里“线程该没有真正创建，t.startLoop（）函数才是真正创建”
  Inspector ins(t.startLoop(), InetAddress(12345), "test"); /*Inspector 的loop是另起的一个Thread创建的， 和main thread 的loop是不一样的，也就是说，现在已经有两个loop了*/
  loop.loop();
}


/*
测试方式：
	执行程序后，在浏览器输入：
		localhost:12345
	会看到浏览器返回如下信息：
		/proc/overview             print basic overview
		/proc/pid                  print pid
		/proc/status               print /proc/self/status
		/proc/threads              list /proc/self/task
		/sys/cpuinfo               print /proc/cpuinfo
		/sys/loadavg               print /proc/loadavg
		/sys/meminfo               print /proc/meminfo
		/sys/overview              print system overview
		/sys/stat                  print /proc/stat
		/sys/version               print /proc/version
	以上信息为HTTP服务器可查询的信息，可以这样使用：
	浏览器输入：
		localhost:12345/proc/pid  则返还服务器pid
		localhost:12345/sys/cpuinfo  则返还服务器CPU信息

*/
