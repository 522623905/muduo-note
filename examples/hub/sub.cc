#include "pubsub.h"
#include <muduo/base/ProcessInfo.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>
#include <vector>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;
using namespace pubsub;

EventLoop* g_loop = NULL;
std::vector<string> g_topics;  //用来存储感兴趣的topic，可有多个，因此用vector

void subscription(const string& topic, const string& content, Timestamp)
{
  printf("%s: %s\n", topic.c_str(), content.c_str());
}

void connection(PubSubClient* client)
{
  if (client->connected())
  {
    for (std::vector<string>::iterator it = g_topics.begin();
        it != g_topics.end(); ++it)
    {
      client->subscribe(*it, subscription); //在此设置回调函数，并未执行
    }
  }
  else
  {
    g_loop->quit();
  }
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    string hostport = argv[1];
    size_t colon = hostport.find(':'); //寻找 ：号的位置
    if (colon != string::npos)
    {
      string hostip = hostport.substr(0, colon); //提取子串，IP地址
      uint16_t port = static_cast<uint16_t>(atoi(hostport.c_str()+colon+1)); //得到端口号
      for (int i = 2; i < argc; ++i)
      {
        g_topics.push_back(argv[i]);  //存topic到vector中，可有多个
      }

      EventLoop loop;
      g_loop = &loop;
      string name = ProcessInfo::username()+"@"+ProcessInfo::hostname(); //获取本机用户名： long@long-boy
      name += ":" + ProcessInfo::pidString();  //+pid  (如：name=long@long-boy:11531)
      PubSubClient client(&loop, InetAddress(hostip, port), name);
      client.setConnectionCallback(connection);
      client.start();
      loop.loop();
    }
    else
    {
      printf("Usage: %s hub_ip:port topic [topic ...]\n", argv[0]);
    }
  }
  else
  {
    printf("Usage: %s hub_ip:port topic [topic ...]\n", argv[0]);
  }
}
