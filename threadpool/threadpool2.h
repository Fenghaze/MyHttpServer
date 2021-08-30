/**
 * @author: fenghaze
 * @date: 2021/07/13 16:55
 * @desc: 线程池，维护一个任务队列
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <vector>
#include <exception>
#include <thread>
#include <mutex>
#include <condition_variable>
template <class T>
class ThreadPool
{
public:
    //懒汉模式
    static ThreadPool<T> &create(int thread_number = 6, int max_requests = 10000)
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
    void worker();

private:
    int m_thread_number;         //线程池中的线程数
    int m_max_requests;          //请求队列中允许的最大请求数
    std::queue<T *> m_workqueue; //任务队列
    bool m_stop;                 //是否结束线程
    std::mutex _lock;
    std::condition_variable _cond;
};

template <class T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests)
{
    m_stop = false;
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    for (int i = 0; i < m_thread_number; i++)
    {
        //printf("create the %dth thread\n", i);
        LOG_INFO << "create the " << i + 1 << "th thread";
        std::thread t(ThreadPool<T>::worker, &this);
        t.detach();
    }
}

template <class T>
ThreadPool<T>::~ThreadPool()
{
    m_stop = true;
}

template <class T>
bool ThreadPool<T>::append(T *task)
{
    unique_lock<std::mutex> lock(_lock);
    while(m_workqueue.size() == m_max_requests)
    {
        _cond.wait(lock);
    }
    m_workqueue.push(task);
    _cond.notify_one();
    return true;
}

template <class T>
void ThreadPool<T>::worker()
{
    while (!m_stop)
    {
        unique_lock<std::mutex> lock(_lock);
        while(m_workqueue.empty())
        {
            _cond.wait();
        }
        T *task = m_workqueue.front();
        m_workqueue.pop();
        _cond.notify_all();
        //执行任务
        task->process();
    }
}

#endif // THREADPOOL_H