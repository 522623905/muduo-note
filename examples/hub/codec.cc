#include "codec.h"

using namespace muduo;
using namespace muduo::net;
using namespace pubsub;


/*
命令如下：
1. sub <topic>\r\n 表示订阅topic
2. unsub <topic>\r\n 表示退订topic
3. pub <topic>\r\n
  输入内容。。。。    表示往订阅了topic的sub发送内容

*/

//解析消息
ParseResult pubsub::parseMessage(Buffer* buf,
                                 string* cmd,
                                 string* topic,
                                 string* content)  //cmd为命令，topic为订阅的感兴趣话题，content为内容
{
  ParseResult result = kError;
  const char* crlf = buf->findCRLF();  //找到\r\n位置（\r\n作为协议分界线）
  if (crlf)
  {
    const char* space = std::find(buf->peek(), crlf, ' '); // 在[ peek(),ctrf]中找出第一个空格位置
    if (space != crlf)   //找到空格
    {
      cmd->assign(buf->peek(), space); //令cmd命令为[peek,space]之间的内容
      topic->assign(space+1, crlf); //得到topic
      if (*cmd == "pub")  //为pub命令，则还需要解析content
      {
        const char* start = crlf + 2; //跳过\r\n字符,即为content开始内容
        crlf = buf->findCRLF(start); //content内容结束地方
        if (crlf)
        {
          content->assign(start, crlf); //得到content内容
          buf->retrieveUntil(crlf+2); //移动buf位置
          result = kSuccess;  //完整的消息
        }
        else
        {
          result = kContinue; //表示content没发送完
        }
      }
      else  //为sub或unsub命令
      {
        buf->retrieveUntil(crlf+2);
        result = kSuccess;
      }
    }
    else
    {
      result = kError;
    }
  }
  else   
  {
    result = kContinue;
  }
  return result;
}

