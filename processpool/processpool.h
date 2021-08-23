/**
 * @author: fenghaze
 * @date: 2021/06/04 17:46
 * @desc:
 * master进程创建一个epoll监听lfd，通过管道分发（round robin算法）任务给子进程，让子进程accept连接
 * 每个个worker进程创建一个epoll监听管道接受accept，之后监听这个客户的cfd
 */

#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string>
#include "listClock.h"
#include "utils.h"
#include "http.h"
static int sig_pipefd[2];

//定时器的回调：关闭非活动连接
void clock_func(int epfd, HTTPConn *user)
{
    //移除节点
    epoll_ctl(epfd, EPOLL_CTL_DEL, user->m_sockfd, 0);
    assert(user);
    close(user->m_sockfd);
    printf("close cfd %d\n", user->m_sockfd);
}

//触发定时器
void time_handler(ListClock list_clock, int epfd)
{
    //调用tick，处理定时器任务
    list_clock.tick(epfd);
    //一次alarm调用只会引起一次SIGALRM信号
    //所以我们要重新定时，以不断触发SIGALRM信号
    alarm(TIMESLOT);
}

static void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    write(sig_pipefd[1], (char *)&msg, 1);
    errno = save_errno;
}

//设置信号处理函数
static void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//子进程类
class WorkerProcess
{
public:
    pid_t m_pid;     //子进程PID
    int m_pipefd[2]; //用于和父进程通信的管道
    int m_clients;   //记录当前连接的客户数量

public:
    WorkerProcess() : m_pid(-1), m_clients(0) {}
};

//进程池
template <typename T>
class ProcessPool
{
public:
    //懒汉模式
    static ProcessPool<T> &create(int lfd, int min_process_num, int max_process_num)
    {
        static ProcessPool<T> mInstance(lfd, min_process_num, max_process_num);
        return mInstance;
    }
    void run();

private:
    ProcessPool(int lfd, int min_process_num, int max_process_num);
    ~ProcessPool();
    void init_sig_pipe();
    void run_master();
    void run_worker();
    int select_worker();

private:
    /*所有进程共享的变量*/
    static const int PER_PROCESS_USER = 65535; //每个woker进程能够连接的最大客户数
    static const int MAX_EVENT_NUMBER = 10000; //每个epoll能够监听的最大事件数
    WorkerProcess *m_workers;                  //worker进程池
    int m_min_process;                         //最小进程数
    int m_max_process;                         //最大进程数
    int m_lfd;                                 //服务类提供的listenfd

    /*master进程和worker进程不同的变量*/
    int m_stop; //每个进程结束的标志
    int m_idx;  //每个worker进程的索引号，master进程的索引号为-1
    int m_epfd; //每个worker进程的epfd句柄
};

template <typename T>
ProcessPool<T>::ProcessPool(int lfd, int min_process_num, int max_process_num) : m_lfd(lfd), m_min_process(min_process_num), m_max_process(max_process_num), m_idx(-1), m_stop(false), m_epfd(-1)
{
    assert((min_process_num > 0) && (min_process_num <= max_process_num));
    //创建空间
    m_workers = new WorkerProcess[m_min_process];
    //初始化
    for (int i = 0; i < min_process_num; i++)
    {
        socketpair(PF_UNIX, SOCK_STREAM, 0, m_workers[i].m_pipefd);
        m_workers[i].m_pid = fork();
        assert(m_workers[i].m_pid >= 0);
        //worker进程关闭socketpair写端，改变m_idx
        if (m_workers[i].m_pid == 0)
        {
            close(m_workers[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
        //master进程关闭socketpair读端
        else
        {
            close(m_workers[i].m_pipefd[1]);
            continue;
        }
    }
}

template <typename T>
ProcessPool<T>::~ProcessPool()
{
    delete[] m_workers;
}

template <typename T>
int ProcessPool<T>::select_worker()
{
    int worker_id = rand() % m_min_process;
    return worker_id;
}

template <typename T>
void ProcessPool<T>::init_sig_pipe()
{
    m_epfd = epoll_create(5);
    assert(m_epfd != -1);
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret == 0);
    setnonblocking(sig_pipefd[1]);
    //监听管道读端
    addfd(m_epfd, sig_pipefd[0]);
    //注册信号
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template <typename T>
void ProcessPool<T>::run_master()
{
    //创建epfd、注册信号、监听信号管道
    init_sig_pipe();
    //父进程额外监听SIGALRM信号
    //addsig(SIGALRM, sig_handler);
    //监听m_lfd
    addfd(m_epfd, m_lfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int worker_id = 0;
    int new_conn = 1;
    int worker_process_number = m_min_process; //当前worker进程
    int all_clients = 0;                       //记录总的连接客户数

    int ret = -1;

    while (!m_stop)
    {
        int n = epoll_wait(m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        for (int i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == m_lfd)
            {
                // int k = worker_id;
                // do
                // {
                //     if (m_workers[k].m_pid != -1)
                //     {
                //         break;
                //     }
                //     k = (k + 1) % worker_id;
                // } while (k != worker_id);
                // if (m_workers[k].m_pid == -1) //woker进程不存在，则不分发任务
                // {
                //     m_stop = true; //master进程停止运行
                //     break;
                // }
                // worker_id = (k + 1) % m_min_process;
                worker_id = select_worker();
                //使用管道通知woker_id进程
                write(m_workers[worker_id].m_pipefd[0], (char *)&new_conn, sizeof(new_conn));
                printf("let worker[%d] process %d accept the connection\n", worker_id, getpid());
            }
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = read(sockfd, signals, sizeof(signals));
                if (ret < 0)
                {
                    continue;
                }
                else
                {
                    for (int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                        // case SIGALRM:
                        // {
                        //     //遍历每个worker进程已连接的客户数
                        //     for (int k = 0; k < m_min_process; k++)
                        //     {
                        //         printf("worker[%d] has %d clients\n", m_workers[k].m_pid, m_workers[k].m_clients);
                        //         all_clients += m_workers[k].m_clients;
                        //     }
                        //     printf("detective %d clients currently!\n", all_clients);
                        //     alarm(1);
                        // }
                        case SIGCHLD: //子进程退出
                        {
                            pid_t pid;
                            int stat;
                            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                            {
                                worker_process_number--;
                                if (worker_process_number == 0)
                                {
                                    m_stop = true;
                                }
                                for (int k = 0; k < m_min_process; k++)
                                {
                                    /*如果进程池中第i个子进程退出了，则主进程关闭相应的通信管道，并设置相应的m_pid为-1，以标记该子进程已经退出*/
                                    if (m_workers[k].m_pid == pid)
                                    {
                                        printf("child %d exit\n", k);
                                        close(m_workers[k].m_pipefd[0]);
                                        m_workers[k].m_pid = -1;
                                    }
                                }
                            }
                            break;
                        }
                        //父进程中断，杀死所有进程
                        case SIGTERM:
                        case SIGINT:
                        {
                            printf("kill all worker process now\n");
                            for (size_t k = 0; k < m_min_process; k++)
                            {
                                int pid = m_workers[k].m_pid;
                                if (pid != -1)
                                {
                                    int sig = -1;
                                    write(m_workers[k].m_pipefd[0], (char *)&sig, sizeof(sig));
                                }
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(m_epfd);
}

template <typename T>
void ProcessPool<T>::run_worker()
{
    //创建epfd、注册信号、监听信号管道
    init_sig_pipe();
    //监听管道读端
    addfd(m_epfd, m_workers[m_idx].m_pipefd[1]);
    //监听定时信号
    addsig(SIGALRM, sig_handler);
    bool timeout = false; //当SIGALRM触发时，变为true，执行定时事件
    epoll_event events[MAX_EVENT_NUMBER];
    //请求服务的客户
    T *users = new T[PER_PROCESS_USER];

    //创建一个定时器容器
    ListClock list_clock;

    assert(users);
    int ret = -1;
    alarm(TIMESLOT); //TIMESLOT秒后触发一次SIGALRM信号,pipefd[0]可读

    while (!m_stop)
    {
        int n = epoll_wait(m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        for (int i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            //管道可读，说明有新客户可以连接，每个客户添加一个定时事件，处理非活动连接
            if ((sockfd == m_workers[m_idx].m_pipefd[1]) && (events[i].events & EPOLLIN))
            {
                int client = 0;
                ret = read(sockfd, (char *)&client, sizeof(client));
                if (((ret < 0) && (errno != EAGAIN || errno != EINTR)) || ret == 0)
                {
                    continue;
                }
                //接受连接
                else if (client == 1)
                {
                    if (m_workers[m_idx].m_clients > PER_PROCESS_USER)
                    {
                        printf("max client limit...\n");
                        continue;
                    }
                    struct sockaddr_in raddr;
                    socklen_t raddr_len = sizeof(raddr);
                    int cfd = accept(m_lfd, (struct sockaddr *)&raddr, &raddr_len);
                    if (cfd < 0)
                    {
                        perror("accept()");
                        continue;
                    }
                    m_workers[m_idx].m_clients += 1;

                    printf("worker %d accept new client....%d\n", getpid(), m_workers[m_idx].m_clients);
                    //监听cfd
                    addfd(m_epfd, cfd);
                    //为该客户初始化服务
                    users[cfd].init(m_epfd, cfd, raddr);

                    //设置定时事件
                    ListNode *node = new ListNode;
                    node->callback = clock_func;
                    node->client_data = &users[cfd];
                    time_t cur = time(nullptr);
                    node->expire = cur + 3 * TIMESLOT; //3 TIMESLOT后关闭

                    users[cfd].m_node = node;

                    list_clock.push(node);
                }
            }
            //有信号
            else if ((sig_pipefd[0] == sockfd) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret < 0)
                    continue;
                else
                {
                    for (size_t j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                        case SIGALRM:
                        {
                            timeout = true;
                            break;
                        }
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
                            {
                                continue;
                            }
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            m_stop = true;
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
            //可读事件
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].Read())
                {
                    //调整定时事件
                    if (users[sockfd].m_node)
                    {
                        time_t cur = time(nullptr);
                        users[sockfd].m_node->expire = cur + 3 * TIMESLOT;
                        printf("adjust timer once\n");
                        list_clock.adjust(users[sockfd].m_node);
                    }
                    users[sockfd].process(); //服务类解析request
                }
                else
                {
                    users[sockfd].close_conn(true);
                    //移除定时事件
                    if (users[sockfd].m_node)
                    {
                        list_clock.pop(users[sockfd].m_node);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                /*根据写的结果，决定是否关闭连接*/
                if (!users[sockfd].Write())
                {
                    users[sockfd].close_conn(true);
                    if (users[sockfd].m_node)
                    {
                        list_clock.pop(users[sockfd].m_node);
                    }
                }
            }
            else //暂时跳过其他事件
            {
                users[sockfd].close_conn(true);
                if (users[sockfd].m_node)
                {
                    list_clock.pop(users[sockfd].m_node);
                }
            }
        }
        //所有事件处理完毕后再执行定时事件
        if (timeout)
        {
            time_handler(list_clock, m_epfd);
            timeout = false;
        }
    }

    delete[] users;
    users = nullptr;
    close(m_workers[m_idx].m_pipefd[0]);
    close(m_epfd);
}

template <typename T>
void ProcessPool<T>::run()
{
    if (m_idx != -1) //worker进程执行这里时，进入if语句
    {
        printf("worker %d run...\n", m_idx);
        run_worker();
        return;
    }
    printf("master %d run...\n", getpid());
    run_master(); //master进程执行这句
}

#endif // PROCESSPOOL_H