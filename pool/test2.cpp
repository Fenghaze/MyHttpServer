/**
 * @author: fenghaze
 * @date: 2021/06/03 11:35
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
#include <pthread.h>
#include <sys/epoll.h>
#include <vector>




class test2
{
private:
    int epfd;
    int lfd;
    std::vector<pthread_t> threads; //线程池，使用vector存储工作线程

    pthread_t mainer;

    pthread_t workertid;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

private:
    test2(int val) : lfd(val) //lfd只有一个，那就是服务器提供的lfd
    {
        threads.resize(30);
        //创建线程池，并初始化每个线程要监听的lfd
        for (int i = 0; i < 30; i++)
        {
            printf("create the NO.%d thread\n", i + 1);
            pthread_create(&threads[i], nullptr, worker, this);
        }

        pthread_create(&mainer, nullptr, manger, this);
    }
    ~test2()
    {
        for (auto &thread : threads)
        {
            pthread_join(thread, nullptr);
        }
        threads.clear();
        threads.shrink_to_fit();
        pthread_join(mainer, nullptr);
    }

    static void *worker(void *arg)
    {
        auto test = (test2 *)arg;
        
    
    }

    static void *manger(void *arg)
    {
        auto test = (test2 *)arg;
        
    }

public:
    static test2 &getInstance(int val)
    {
        static test2 m_instance(val);
        return m_instance;
    }
};

int main(int argc, char const *argv[])
{
    auto &app = test2::getInstance(3);
    return 0;
}