/**
 * @author: fenghaze
 * @date: 2021/07/15 11:33
 * @desc: 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include "HttpServer.h"
#include "threadpool.h"
#include "../utils/utils.h"
#include "../lock/locker.h"

#define SERVERPORT "8888"
#define MAX_EVENT_NUM 10000
#define MAX_FD 65535

std::map<std::string, std::string> users; //保存post请求体的数据
locker m_userslock;                       //保护users临界资源

//创建并监听lfd
int initSocket(int &lfd, struct sockaddr_in laddr)
{
    users["123"] = "123"; //初始化一条数据：用户名、密码

    int ret;
    //创建lfd，配置lfd
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(SERVERPORT));
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);

    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //绑定
    ret = bind(lfd, (struct sockaddr *)&laddr, sizeof(laddr));
    assert(ret >= 0);

    //监听
    ret = listen(lfd, 127);
    assert(ret >= 0);

    std::cout << "lfd = " << lfd << std::endl;
}

int main(int argc, char const *argv[])
{
    int lfd;                  //监听HttpServer的lfd
    struct sockaddr_in laddr; //服务端sockert地址
    int epfd;                 //监听所有socket的epoll
    epoll_event events[MAX_EVENT_NUM];

    //创建用于HTTP服务的线程池
    ThreadPool<HttpServer> &threadpool = ThreadPool<HttpServer>::create();

    // //预先为每个可能的客户连接分配一个 HttpServer 对象
    HttpServer *users = new HttpServer[MAX_FD];

    initSocket(lfd, laddr);

    epfd = epoll_create(1);
    assert(epfd != -1);
    HttpServer::m_epollfd = epfd;

    //将lfd注册到epfd上
    addfd(epfd, lfd, false);

    while (1)
    {
        int n = epoll_wait(epfd, events, MAX_EVENT_NUM, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            exit(1);
        }
        for (int i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == lfd)
            {
                struct sockaddr_in raddr;
                socklen_t raddr_len = sizeof(raddr);
                int cfd = accept(sockfd, (struct sockaddr *)&raddr, &raddr_len);
                if (cfd < 0)
                {
                    fprintf(stdout, "errno is :%d\n", errno);
                    continue;
                }
                users[cfd].init(cfd, raddr);
                printf("accept new client %d..\n", HttpServer::m_user_count);
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read())
                {
                    //若监测到读事件，将该事件放入请求队列，线程池有任务后会执行process()
                    //process()负责处理http request和http response
                    threadpool.append(users + sockfd);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (users[sockfd].write())
                {
                    std::cout << "send data to the client " << sockfd << std::endl;
                }
            }
            else
            {
                std::cout << "something else" << std::endl;
            }
        }
    }
    close(epfd);
    close(lfd);
    delete[] users;
    return 0;
}