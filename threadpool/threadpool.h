/**
 * @author: fenghaze
 * @date: 2021/07/13 16:55
 * @desc: 线程池，维护一个任务队列
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <vector>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"

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
    //工作线程运行的函数，内部调用run()
    static void *worker(void *arg);
    //线程调用的函数，即线程创建后就一直等待信号量m_queuestat，从任务队列中取任务来执行
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
    m_stop = false;
    m_threads = nullptr;
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < m_thread_number; i++)
    {
        printf("create the %dth thread\n", i);
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

template <class T>
bool ThreadPool<T>::append(T *task)
{
    m_queuelocker.lock();
    //超过任务限制数，则报错
    if(m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(task);
    m_queuelocker.unlock();
    //信号量增加
    m_queuestat.post();
    return true;
}

template <class T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool *threadpool = (ThreadPool *)arg;
    threadpool->run();
    return threadpool;
}

template <class T>
void ThreadPool<T>::run()
{
    while (!m_stop)
    {
        //等待信号量
        m_queuestat.wait();
        //从任务队列中取出任务时需要加锁
        m_queuelocker.lock();
        //队列为空，继续等待
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *task = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!task)
        {
            continue;
        }
        //执行任务
        task->process(); 
    }
}

#endif // THREADPOOL_H