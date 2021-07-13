/**
 * @author: fenghaze
 * @date: 2021/07/13 16:55
 * @desc: 线程池，维护一个任务队列，当队列空时，等待
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <vector>
#include <exception>
#include <pthread.h>

using namespace clog;

template <class T>
class ThreadPool
{
public:
    //懒汉模式
    static ThreadPool<T> &create(int thread_number = 8, int max_requests = 10000)
    {
        static ThreadPool<T> mInstance(thread_number, max_requests);
        return mInstance;
    }

    //向任务队列中添加T类型任务
    bool append(T *task);

private:
    //创建工作线程，并分离
    ThreadPool(int thread_number, int max_requests);
    ~ThreadPool();
    //工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void *worker(void *arg);
    //运行工作线程
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //任务队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
};

template <class T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < m_thread_number; i++)
    {
        printf("create the %dth thread\n",i);
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <class T>
ThreadPool<T>::~ThreadPool()
{
    delete[] m_threads;
    m_stop = true;
}

#endif // THREADPOOL_H