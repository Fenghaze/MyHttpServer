/**
 * @author: fenghaze
 * @date: 2021/06/01 15:05
 * @desc: 懒汉模式创建线程池类
 * 成员变量：线程池的存储结构、子线程的个数、分发任务的任务队列
 * 构造函数：初始化线程池，每个子线程进行线程分离
 * 成员函数：
 * - run()：运行每个
 * - void *worker(void *arg)：线程函数
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <vector>
#include "../lock/lock.h"

template<class T>
class ThreadPool
{
private:
    ThreadPool();
    ~ThreadPool();

    void run();
    static void *worker(void *arg);

public:
    ThreadPool &getInstance()
    {
        static ThreadPool thread_pool;
        return thread_pool;
    }

private:
    int m_thread_number;              //线程数量
    int m_maxsize;                    //线程最大数量，用于动态扩容
    std::vector<pthread_t> m_threads; //线程池

    std::list<T *> m_workqueue; //任务队列
    locker m_mutex; //保护任务队列的互斥锁
    sem m_sem;      //任务队列的信号量

};

ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
}





#endif // THREADPOOL_H