# 线程同步
对`<pthread.h>`库中的**互斥锁**、**条件变量**，和`<semaphore.h>`库中的**信号量**进行封装。

以上三种方法能够保证线程安全，每个方法的作用如下：

- 互斥量（互斥锁）：**保护临界区**
- 条件变量（条件锁）：**线程间通知机制**，当某个共享资源达到某个值的时候，唤醒等待这个共享资源的线程
- 信号量（共享锁）：**允许有多个线程来访问资源**，但是需要限定访问资源的线程个数，达到资源共享的目的

