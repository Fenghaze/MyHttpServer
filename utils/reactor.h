/**
 * @author: fenghaze
 * @date: 2021/06/05 15:06
 * @desc: 
 */

#ifndef REACTOR_H
#define REACTOR_H

#define BUFFERSIZE 10

//自定义事件驱动，epoll.data.ptr指向这个事件驱动
class MyEvent
{
public:
    int fd;                                          // 要监听的文件描述符
    int events;                                      // 对应的监听事件
    void *arg;                                       // 泛型参数
    void (*callback)(int fd, int events, void *arg); // 回调函数
    int status;                                      // 是否在监听：1表示在红黑树上，0表示不在红黑树上
    char buf[BUFFERSIZE];                            // 缓冲区
    int len;                                         // 长度
    long last_active;                                // 记录每次fd加入红黑树 efd 的时间，类似超时设置
};

class EventReactor
{

public:
    void addEvent(MyEvent *my_event, int epfd, int event)
    {
    }

    void setEvent(MyEvent *my_event, int fd, void (*callback)(int fd, int events, void *arg), void *arg)
    {
    }

    void delEvent(MyEvent *my_event, int epfd)
    {
    }

    void (*initAccept)(int fd, int events, void *arg)
    {
    }

    void (*recvData)(int fd, int events, void *arg)
    {
    }

    void (*sendData)(int fd, int events, void *arg)
    {
    }
};


#endif // REACTOR_H