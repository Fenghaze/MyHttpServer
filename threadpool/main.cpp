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
#include <thread>
#include "HttpServer.h"
#include "threadpool.h"
#include "../utils/utils.h"
#include "../lock/locker.h"
#include "../log/AsyncLogger.h"
#include "../log/Logger.h"
#include "../log/Localtime.h"
#include "../clock/minheap.h"

#define SERVERPORT "8888"
#define MAX_EVENT_NUM 655350
#define MAX_CLIENTS 300000

std::map<std::string, std::string> users; //保存post请求体的数据
locker m_userslock;                       //保护users临界资源

static int pipefd[2];
static int epfd = 0; //监听所有socket的epoll

//信号处理函数
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

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

    //std::cout << "lfd = " << lfd << std::endl;
    LOG_INFO << "lfd =" << lfd;
}

void tf1()
{
    LOG_INFO << "create logfile......";
}

int main(int argc, char const *argv[])
{

    using namespace clog;
    Logger::setLogLevel(Logger::TRACE);
    Logger::setConcurrentMode();
    Localtime begin(Localtime::now());

    //必须创建一个线程后，异步日志才能正常使用
    std::thread t(tf1);
    t.join();

    int lfd;                  //监听HttpServer的lfd
    struct sockaddr_in laddr; //服务端sockert地址
    epoll_event events[MAX_EVENT_NUM];

    //创建用于HTTP服务的线程池
    ThreadPool<HttpServer> &threadpool = ThreadPool<HttpServer>::create();

    // //预先为每个可能的客户连接分配一个 HttpServer 对象
    HttpServer *users = new HttpServer[MAX_CLIENTS];

    initSocket(lfd, laddr);

    epfd = epoll_create(1);
    assert(epfd != -1);
    HttpServer::m_epollfd = epfd;
    //将lfd注册到epfd上
    addfd(epfd, lfd, false);

    //创建管道
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epfd, pipefd[0], false);

    addsig(SIGTERM, sig_handler, false);

    bool stop_server = false;
    while (!stop_server)
    {
        int n = epoll_wait(epfd, events, MAX_EVENT_NUM, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            LOG_ERROR << "epoll_wait()" << errno;
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
                    //fprintf(stdout, "errno is :%d\n", errno);
                    LOG_ERROR << "accpet() errno " << errno;
                    continue;
                }
                if (HttpServer::m_user_count >= MAX_CLIENTS)
                {
                    printf("Internal server busy\n");
                    LOG_ERROR << "Internal server busy";
                    continue;
                }
                users[cfd].init(cfd, raddr);
                printf("accept %dth new client ..\n", HttpServer::m_user_count);
                LOG_INFO << "accept " << HttpServer::m_user_count << "th new client ..";
            }
            //处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            //处理客户连接上接收到的数据，发生可读事件，定时器延长
            else if (events[i].events & EPOLLIN)
            {
                //auto timer = users_clocks[sockfd].timer;
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
                    std::cout << "send data to the client fd=" << sockfd << std::endl;
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
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    double times = timeDifference(Localtime::now(), begin);
    printf("Time is %10.4lf s\n", times);
    return 0;
}