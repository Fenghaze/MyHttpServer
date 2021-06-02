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

    pthread_t m_manager;              //管理者线程，管理线程池
    pthread_t m_main;                 //主线程，负责接受连接和分配cfd
    std::vector<pthread_t> m_threads; //线程池，使用vector存储工作线程

    bool m_shutdown; //标识是否关闭线程

    int m_lfd;                                 //服务端的监听socket
    int m_cfd;                                 //连接socket，所有线程共享
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
    //创建线程池
    for (int i = 0; i < min_thread_number; i++)
    {
        printf("create the NO.%d thread\n", i + 1);
        pthread_create(&m_threads[i], nullptr, worker, this);
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
        pthread_join(thread, nullptr);
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

//工作线程函数：运行run
template <typename T>
void *EfficientThreadPool<T>::worker(void *arg)
{
    auto pool = (EfficientThreadPool *)arg;
    //创建自己的epoll表
    if (pool->m_lfd < 0)
    {
        return nullptr;
    }
    pool->m_epfd = epoll_create(1);
    printf("sub thread %d create epollfd %d\n", gettid(), pool->m_epfd);
    addfd(pool->m_epfd, pool->m_lfd);
    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    int ret = -1;
    while (true)
    {
        int n = epoll_wait(pool->m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        for (size_t i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            //接收新客户连接
            if ((sockfd == pool->m_lfd) && (events[i].events & EPOLLIN))
            {
                int client;
                struct sockaddr_in raddr;
                socklen_t raddr_len = sizeof(raddr);
                int cfd = accept(pool->m_lfd, (struct sockaddr *)&raddr, &raddr_len);
                if (cfd < 0)
                {
                    perror("accept()");
                    continue;
                }
                //监听cfd
                addfd(pool->m_epfd, cfd);
                //添加新客户
                users[cfd].init(pool->m_epfd, cfd, raddr);
            }
            else if (events[i].events & EPOLLIN)
            {
                //users[sockfd].process();
                continue;
            }
            else
            {
                continue;
            }
        }
    }
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
    printf("main thread %d create epollfd %d\n", gettid(), pool->m_epfd);
    addfd(pool->m_epfd, pool->m_lfd);
    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int sub_thread_counter = 0;
    while (true)
    {
        int n = epoll_wait(pool->m_epfd, events, MAX_EVENT_NUMBER, -1);
        if ((n < 0) && (errno != EINTR))
        {
            perror("epoll_wait()");
            break;
        }
        for (size_t i = 0; i < n; i++)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == pool->m_lfd) //有新客户，采用Round Robin方式分配子线程
            {
                int k = sub_thread_counter;
                do
                {
                    // if (pool->m_sub_process[k].m_pid != -1)
                    // {
                    //     break;
                    // }
                    k = (k + 1) % sub_thread_counter;
                } while (k != sub_thread_counter);
                //分配工作线程序号
                sub_thread_counter = (k + 1) % pool->live_thread_number;
                //通知这个工作线程有新的连接到来

                printf("send cfd to child %d\n", sub_thread_counter);
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