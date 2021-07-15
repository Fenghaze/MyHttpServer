/**
 * @author: fenghaze
 * @date: 2021/07/15 09:47
 * @desc: Http服务，负责处理IO
 */

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <errno.h>
#include <sys/uio.h>
#include <iostream>
#include "HttpRequest.h"
#include "HttpResponse.h"

class HttpServer
{
public:
    HttpServer() {}
    ~HttpServer() {}

public:
    //初始化客户连接：获得客户信息，并添加到m_epollfd
    void init(int cfd, struct sockadd_in &addr);

    void init(int epfd, int cfd, struct sockadd_in &addr);

    //IO处理函数：解析http requset，响应http response
    void process();

    //关闭连接
    void close_conn(bool real_close = true);

public:
    /*线程池模型中，所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态的，
    在main线程中进行初始化*/
    static int m_epollfd;

    /*进程池模型中，worker进程监听的socket被注册到不同的epoll内核事件表中，
    调用void init(int epfd, int cfd, struct sockadd_in &addr)初始化*/
    int _epfd;

    static int m_user_count; //统计用户数量
    

private:
    int m_sockfd;            //用于通信的连接cfd
    struct sockaddr_in addr; //socket地址

    HttpRequest request;
    HttpResponse response;
};




#endif // HTTPSERVER_H