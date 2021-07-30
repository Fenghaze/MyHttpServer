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
#include <signal.h>
#include "HttpServer.h"
#include "threadpool.h"
#include "../utils/utils.h"
#include "../lock/locker.h"
#include "../clock/timeHeap.h"

#define SERVERPORT "8888"
#define MAX_EVENT_NUM 10000
#define MAX_FD 65535

std::map<std::string, std::string> users; //保存post请求体的数据
locker m_userslock;                       //保护users临界资源

static int sig_pipefd[2]; //信号管道，统一事件源

void sig_handler(int sig)
{
    int save_errnor = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *)&msg, 1, 0); //从管道写端发送信号
    errno = save_errnor;
}
//定时器回调函数，删除非活动连接在socket上的注册事件，并关闭
void cb_func(int epfd, client_data *client)
{
    delfd(epfd, client->sockfd);
    HttpServer::m_user_count--;
    printf("close fd %d\n", client->sockfd);
    //LOG_INFO("close fd %d", client_data->sockfd);
    //Log::get_instance()->flush();
}

void add_sig(int sig, void(handler)(int), bool restart = true)
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

    std::cout << "lfd = " << lfd << std::endl;
}

int main(int argc, char const *argv[])
{
    int lfd;                  //监听HttpServer的lfd
    struct sockaddr_in laddr; //服务端sockert地址
    int epfd;                 //监听所有socket的epoll
    epoll_event events[MAX_EVENT_NUM];
    TimeHeap heaps; //定时器容器

    //创建用于HTTP服务的线程池
    ThreadPool<HttpServer> &threadpool = ThreadPool<HttpServer>::create();

    //预先为每个可能的客户连接分配一个 HttpServer 对象
    HttpServer *users = new HttpServer[MAX_FD];
    client_data *users_timer = new client_data[MAX_FD];

    //初始化socket
    initSocket(lfd, laddr);

    epfd = epoll_create(1);
    assert(epfd != -1);
    HttpServer::m_epollfd = epfd;
    //将lfd注册到epfd上
    addfd(epfd, lfd, false);

    //监听信号
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd(epfd, sig_pipefd[0], false); //监听管道读端
    add_sig(SIGALRM, sig_handler, false);
    add_sig(SIGTERM, sig_handler, false);
    bool timeout = false;     //是否执行定时事件
    bool stop_server = false; //是否终止进程
    //设置定时信号,TIMEOUT后触发
    alarm(TIMEOUT);

    while (!stop_server)
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
                //设置定时器
                users_timer[cfd].addr = raddr;
                users_timer[cfd].sockfd = cfd;
                HeapNode *timer = new HeapNode(3 * TIMEOUT);
                timer->client = &users_timer[cfd];
                timer->callback = cb_func;
                heaps.push(timer);
                users_timer[cfd].timer = timer;
                printf("accept %dth new client ..\n", HttpServer::m_user_count);
            }
            //处理信号
            if ((events[i].events & EPOLLIN) && (sockfd == sig_pipefd[0]))
            {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; i++)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM:
                        {
                            timeout = true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                            break;
                        }
                        }
                    }
                }
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                //获得客户的定时器节点
                auto timer = users_timer[sockfd].timer;
                printf("timer size %d\n", heaps.size());
                if (users[sockfd].read())
                {
                    //若监测到读事件，将该事件放入请求队列，线程池有任务后会执行process()
                    //process()负责处理http request和http response
                    threadpool.append(users + sockfd);
                    printf("EPOLLIN：adjust timer once\n");

                    //若有数据传输，则将定时器往后延迟3*TIMEOUT个单位
                    if (timer)
                    {
                        timer->expire = time(nullptr) + 3 * TIMEOUT;
                        printf("EPOLLIN：adjust timer once\n");
                        heaps.adjust(timer);
                    }
                }
                //没有数据可读，则删除该定时器
                else
                {
                }
            }
            //写事件
            else if (events[i].events & EPOLLOUT)
            {
                if (users[sockfd].write())
                {
                    //std::cout << "send data to the client fd=" << sockfd << std::endl;
                }
            }
            else
            {
                std::cout << "something else" << std::endl;
            }
        }
        if (timeout) //处理完所有事件后再执行定时事件
        {
            heaps.tick(epfd); //执行一次tick
            alarm(TIMEOUT);   //再一次设置定时信号
            timeout = false;
        }
    }
    close(epfd);
    close(lfd);
    close(sig_pipefd[1]);
    close(sig_pipefd[0]);
    delete[] users;
    return 0;
}