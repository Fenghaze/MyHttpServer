/**
 * @author: fenghaze
 * @date: 2021/06/02 10:14
 * @desc: 线程池类的实现
 * 成员函数：
 * - 初始化线程池：创建n个工作线程、1个管理者线程
 * - 获得一个工作线程：从线程池中pop一个线程
 * - 回收一个工作线程：向线程池中push一个指定线程
 * - 动态缩小线程池：摧毁空闲线程（cfd关闭连接，线程回收后变为空闲线程）
 * - 动态扩充线程池：live线程 == max时，再创建min个新的线程
 * - 销毁线程池：清空线程池
 * - 工作线程函数：自己的epoll只监听cfd和信号，处理lfd的IO事件
 * - 管理者线程函数：定时对线程池进行管理，需要使用互斥锁进行写操作
 */

#ifndef EFFICIENT_PROCESSPOOL_H
#define EFFICIENT_PROCESSPOOL_H
#include <memory>
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
#include <pthread.h>
#include "../lock/lock.h"

class Threads
{
public:
    pthread_t m_tid; //子线程号
    int m_pipefd[2]; //和主线程通信的管道
    int m_epfd;      //每个线程都有一个epoll内核事件表
    int lfd;

public:
    Threads() : m_tid(-1), m_epfd(-1) {}
};

/*************************************************/
/*******************工具函数***********************/
/*************************************************/

//fd设置为非阻塞
static int setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

//epoll添加节点
static void addfd(int epfd, int fd)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

//删除节点
static void removefd(int epfd, int fd)
{

    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/*************************************************/
/*******************工具函数***********************/
/*************************************************/

template <typename T>
class EfficientThreadPool
{
public:
    static EfficientThreadPool<T> &create(int lfd, int min_threads = 5, int max_threads = 10)
    {
        static EfficientThreadPool<T> m_instance(lfd, min_threads, max_threads);
        return m_instance;
    }

    //获得一个工作线程
    std::shared_ptr<pthread_t> getThread();
    //回收一个工作线程
    void retThread(std::shared_ptr<pthread_t> &tid);

private:
    //初始化线程池：创建min_thread_number个线程，1个管理线程
    EfficientThreadPool(int lfd, int min_threads, int max_threads);
    //摧毁线程池
    ~EfficientThreadPool();
    // //动态缩小线程池：摧毁空闲线程（cfd关闭连接，线程回收后变为空闲线程）
    // void reducePool();
    // //动态扩充线程池：live线程 == max时，再创建min个新的线程
    // void expandPool();
    //工作线程函数：自己的epoll只监听cfd和信号，处理lfd的IO事件
    void run();
    //工作线程函数：运行run
    static void *worker(void *arg);
    //管理者线程函数：定时对线程池进行管理，需要使用互斥锁进行写操作
    static void *manager(void *arg);
    //主线程函数：负责监听lfd和信号，当lfd可读时，发送给工作线程
    static void *main_worker(void *arg);

    //统一信号事件源
    void setup_sig_pipe();

private:
    int min_thread_number;  /*线程池最小线程数*/
    int max_thread_number;  /*线程池最大线程数*/
    int live_thread_number; /*当前存活的线程数*/
    int busy_thread_number; /*工作中的线程数*/
    int exit_thread_number; /*要摧毁的线程数*/

    pthread_t m_manager;            //管理者线程，管理线程池
    pthread_t m_main;               //主线程，负责接受连接和分配cfd
    std::vector<Threads> m_threads; //线程池，使用vector存储工作线程

    bool m_shutdown; //标识是否关闭线程
    int worker_id;
    int m_lfd;                                 //服务端的监听socket
    int m_epfd;                                //每个线程都有一个epoll内核事件表
    static const int USER_PER_PROCESS = 65536; /*工作线程最多能处理的客户数量*/
    static const int MAX_EVENT_NUMBER = 10000; /*epoll最多能处理的事件数*/
};

template <typename T>
EfficientThreadPool<T>::EfficientThreadPool(int lfd, int min_threads, int max_threads) : m_lfd(lfd), min_thread_number(min_threads), max_thread_number(max_threads)
{
    //初始化其他成员变量
    live_thread_number = min_thread_number;
    busy_thread_number = 0;
    exit_thread_number = 0;
    m_shutdown = false;
    if ((min_threads <= 0))
    {
        throw std::exception();
    }
    //初始化线程池
    m_threads.resize(min_thread_number);
    //创建线程池，并初始化每个线程要监听的lfd
    for (int i = 0; i < min_thread_number; i++)
    {
        printf("create the NO.%d thread\n", i + 1);
        pipe(m_threads[i].m_pipefd);
        m_threads[i].lfd = m_lfd;
        pthread_create(&m_threads[i].m_tid, nullptr, worker, &m_threads[i]);
    }
    //创建管理者线程
    pthread_create(&m_manager, nullptr, manager, this);

    //创建主线程
    pthread_create(&m_main, nullptr, main_worker, this);
    //主线程分离
    pthread_detach(m_main);
}
//摧毁线程池
template <typename T>
EfficientThreadPool<T>::~EfficientThreadPool()
{
    //摧毁管理者线程
    pthread_join(m_manager, nullptr);

    for (auto &thread : m_threads)
    {
        pthread_join(thread.m_tid, nullptr);
    }
    m_threads.clear();
    m_threads.shrink_to_fit();
}

// //动态缩小线程池：摧毁空闲线程（cfd关闭连接，线程回收后变为空闲线程）
// template <typename T>
// void EfficientThreadPool<T>::reducePool(int size)
// {
// }

// //动态扩充线程池：live线程 == max时，再创建min个新的线程
// template <typename T>
// void EfficientThreadPool<T>::expandPool(int size)
// {
// }

//工作线程函数：自己的epoll只监听cfd和信号，处理lfd的IO事件
template <typename T>
void EfficientThreadPool<T>::run()
{
}

//工作线程函数：不断接受新的连接，并监听IO事件
template <typename T>
void *EfficientThreadPool<T>::worker(void *arg)
{
    auto current_worker = *(Threads *)arg;
    //创建自己的epoll表
    current_worker.m_epfd = epoll_create(1);
    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    printf("sub thread %ld, lfd=%d, epfd=%d\n", pthread_self(), current_worker.lfd, current_worker.m_epfd);
    int ret = -1;
    int pipefd = current_worker.m_pipefd[0];
    addfd(current_worker.m_epfd, pipefd);
    while (true)
    {
        int n = epoll_wait(current_worker.m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        for (size_t i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN))
            {
                int client;
                ret = read(sockfd, (char *)&client, sizeof(client));
                //ret = recv(sockfd, (char *)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0)
                {
                    continue;
                }
                else
                {
                    struct sockaddr_in raddr;
                    socklen_t raddr_len = sizeof(raddr);
                    int cfd = accept(current_worker.lfd, (struct sockaddr *)&raddr, &raddr_len);
                    if (cfd < 0)
                    {
                        perror("accept()");
                        continue;
                    }
                    //监听cfd
                    addfd(current_worker.m_epfd, cfd);
                    //添加新客户
                    users[cfd].init(current_worker.m_epfd, cfd, raddr);
                    printf("thread  %ld accept new client...\n", pthread_self());
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                users[sockfd].process();
                printf("thread %ld epollin do something...\n", pthread_self());
            }
            else
            {
                continue;
            }
        }
    }
    delete[] users;
    users = nullptr;
    close(current_worker.m_epfd);
    close(pipefd);
}

//管理者线程函数：定时对线程池进行管理，需要使用互斥锁进行写操作
template <typename T>
void *EfficientThreadPool<T>::manager(void *arg)
{
    auto pool = (EfficientThreadPool *)arg;
    printf("hello manager\n");
    return pool;
}

//主线程函数：负责监听lfd和信号，当lfd可读时，运行工作线程
template <typename T>
void *EfficientThreadPool<T>::main_worker(void *arg)
{
    auto pool = (EfficientThreadPool *)arg;
    pool->m_epfd = epoll_create(1);
    printf("main worker thread %ld, lfd=%d, epfd=%d\n", pthread_self(), pool->m_lfd, pool->m_epfd);

    addfd(pool->m_epfd, pool->m_lfd);
    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int k = 0;
    while (true)
    {
        int n = epoll_wait(pool->m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        int new_conn = 1;
        for (size_t i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == pool->m_lfd) //有新客户，采用随机数的方式分配子线程
            {
                k = rand() % pool->live_thread_number;
                //工作线程的tid
                pool->worker_id = k;
                //通知这个工作线程有新的连接到来
                printf("wokertid[%d]\n", k);
                //epoll检测到test->lfd有可读事件，说明有新客户，通知woker_id线程来接受连接
                //send(pool->m_threads[k].m_pipefd[1], (char *)&new_conn, sizeof(new_conn), 0);
                write(pool->m_threads[k].m_pipefd[1], (char *)&new_conn, sizeof(new_conn));
            }
            else
            {
                continue;
            }
        }
    }
}

//获得一个工作线程
template <typename T>
std::shared_ptr<pthread_t> EfficientThreadPool<T>::getThread()
{
}

//回收一个工作线程
template <typename T>
void EfficientThreadPool<T>::retThread(std::shared_ptr<pthread_t> &tid)
{
}

#endif // EFFICIENT_PROCESSPOOL_H