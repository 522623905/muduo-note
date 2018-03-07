// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/http/HttpResponse.h>
#include <muduo/net/Buffer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 将http响应信息添加到Buffer
void HttpResponse::appendToBuffer(Buffer* output) const
{
  char buf[32];
  snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);  //以下几行表示：构造响应行，如HTTP/1.0 200 OK
  output->append(buf);
  output->append(statusMessage_);
  output->append("\r\n");

  if (closeConnection_)
  {
      //该字段告诉服务器处理完这个连接后关闭
    output->append("Connection: close\r\n");
  }
  else
  {
      //继续保持连接
    snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());  //目标文档的长度
    output->append(buf);
    output->append("Connection: Keep-Alive\r\n");   //处理完之后仍然保持连接
  }

  //遍历响应头部字段
  for (std::map<string, string>::const_iterator it = headers_.begin();
       it != headers_.end();
       ++it)
  {
    output->append(it->first);    //头部字段名称
    output->append(": ");         //注意，是冒号+空格
    output->append(it->second);   //头部值
    output->append("\r\n");       //每个字段以\r\n结束
  }

  output->append("\r\n");   //头部字段结束后，必须加一个空行
  output->append(body_);    //具体的响应报文，即要显示在客户端浏览器的内容,要使用html格式编写
}
