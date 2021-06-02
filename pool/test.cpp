// /**
//  * @author: fenghaze
//  * @date: 2021/06/02 16:34
//  * @desc:
//  */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

int cfd = 3;

void sig_handler(int signo)
{
    switch (signo)
    {
    case SIGUSR1:
        printf("thread %d SIGUSR1\n", gettid());
        fflush(nullptr);
        break;
    case SIGUSR2:
        printf("thread %d SIGUSR2\n", gettid());
        fflush(nullptr);
        break;
    case SIGINT:
        printf("thread %d SIGINT\n", gettid());
        fflush(nullptr);
        break;
    default:
        printf("other signal\n");
        fflush(nullptr);
        break;
    }
}

static void *func(void *arg)
{
    printf("thread : %d\n", gettid());
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);
    pthread_exit(nullptr);
}

int main(int argc, char const *argv[])
{
    pthread_t *threads;
    for (int i = 0; i < 3; i++)
    {
        printf("create the NO.%d thread\n", i + 1);
        pthread_create(threads + i, nullptr, func, &i);
    }

    pthread_kill(threads[0], SIGUSR1);
    pthread_kill(threads[1], SIGUSR2);
    pthread_kill(threads[2], SIGUSR1);

    for (int i = 0; i < 3; i++)
    {
        pthread_join(threads[i], nullptr);
    }

    //main线程发送数据给子线程
    

    return 0;
}
