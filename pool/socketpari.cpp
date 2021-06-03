/*
 * Pipe.cpp
 *
 *  Created on: 2014年6月10日
 *      Author: 
 */
 
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <vector>
 
using namespace std;
 


class Threads
{
public:
    pthread_t m_tid; //子线程号
    int m_pipefd[2]; //和主线程通信的管道
public:
    Threads() : m_tid(-1) {}
};

int d[2];
 
void * take(void * arg)
{
    auto consumer = *(Threads *)arg;
    char buffer[100];
    read(consumer.m_pipefd[0],buffer,100);
    printf("thread %ld get data %s\n", pthread_self(), buffer);
    return NULL;
}
 
void * put(void * arg)
{
	write(d[1],"hello pipe!",20);
	cout << "write data " <<endl;
 
	return NULL;
}
 
int main()
{
	// void* ret;
	// pthread_t customer1, producer;
	// pipe(d);
 
	// pthread_create(&customer1, NULL, take, NULL);
 
	// sleep(2);
 
	// pthread_create(&producer, NULL, put, NULL);
 
	// pthread_join(customer1, &ret);
	// pthread_join(producer, &ret);

    void* ret;
	Threads customer1, customer2;
	pipe(customer1.m_pipefd);
	pipe(customer2.m_pipefd);

	pthread_create(&customer1.m_tid, NULL, take, &customer1);
	pthread_create(&customer2.m_tid, NULL, take, &customer2);

	sleep(2);

    write(customer1.m_pipefd[1],"hello pipe!",20);
    printf("main thread write data to thread %d\n", 0);

    //pthread_create(&producer.m_tid, NULL, put, NULL);
 
	pthread_join(customer1.m_tid, &ret);
	pthread_join(customer2.m_tid, &ret);
	//pthread_join(producer.m_tid, &ret);
    

    return 0;
}
 
 