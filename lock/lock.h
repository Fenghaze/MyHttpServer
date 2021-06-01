/**
 * @author: fenghaze
 * @date: 2021/06/01 14:45
 * @desc: 对互斥锁、条件变量和信号量进行封装
 */

#ifndef LOCK_H
#define LOCK_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>


/*封装信号量的类：信号量的作用是可以让多个线程读取资源，达到资源共享*/
class sem
{
public:
    /*创建并初始化信号量*/
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            /*构造函数没有返回值，可以通过抛出异常来报告错误*/
            throw std::exception();
        }
    }
    /*销毁信号量*/
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    /*等待信号量*/
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    /*增加信号量*/
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};
/*封装互斥锁的类：互斥锁的作用是保护某个资源，某时刻只允许一个线程访问资源*/
class locker
{
public:
    /*创建并初始化互斥锁*/
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    /*销毁互斥锁*/
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    /*获取互斥锁*/
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    /*释放互斥锁*/
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

private:
    pthread_mutex_t m_mutex;
};
/*封装条件变量的类：条件变量通常与互斥锁一起使用，当资源可用时，用于通知其他线程来竞争资源*/
class cond
{
public:
    /*创建并初始化条件变量*/
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            /*构造函数中一旦出现问题，就应该立即释放已经成功分配了的资源*/
            throw std::exception();
        }
    }
    /*销毁条件变量*/
    ~cond()
    {
        //pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }
    /*等待条件变量*/
    bool wait(pthread_mutex_t *mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, mutex);
        return ret == 0;
    }
    /*唤醒等待条件变量的线程*/
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

private:
    //pthread_mutex_t m_mutex;    /*条件变量的互斥锁*/
    pthread_cond_t m_cond;      /*条件变量*/
};

#endif // LOCK_H