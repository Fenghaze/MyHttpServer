/**
 * @author: fenghaze
 * @date: 2021/06/05 15:06
 * @desc: 
 */

#ifndef REACTOR_H
#define REACTOR_H

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#define BUFFERSIZE 2048

int setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

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

void initEvent(MyEvent *my_event, int fd, void (*callback)(int fd, int events, void *arg), void *arg)
{
    my_event->fd = fd;
    my_event->callback = callback;
    my_event->arg = arg;
    my_event->events = 0;
    my_event->status = 0;
    if (my_event->len <= 0)
    {
        memset(my_event->buf, 0, sizeof(my_event->buf));
        my_event->len = 0;
    }
    my_event->last_active = time(NULL);
}

void addEvent(MyEvent *my_event, int epfd, int event)
{
    epoll_event ev;
    ev.data.ptr = my_event;
    ev.events = my_event->events = event;
    int op = 0;
    if (my_event->status == 0)
    {
        op = EPOLL_CTL_ADD;
        my_event->status = 1;
    }
    epoll_ctl(epfd, op, my_event->fd, &ev);
}

void modEvent(MyEvent *my_event, int epfd, int event)
{
    epoll_event ev;
    ev.data.ptr = my_event;
    ev.events = my_event->events = event;
    epoll_ctl(epfd, EPOLL_CTL_MOD, my_event->fd, &ev);
}

void delEvent(MyEvent *my_event, int epfd)
{
    epoll_event ev;
    if (my_event->status != 1)
    {
        return;
    }
    ev.data.ptr = nullptr;
    ev.events = my_event->events = 0;
    my_event->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, my_event->fd, &ev);
}

void (*connAccept)(int fd, int events, void *arg)
{
    
}

void (*recvData)(int fd, int events, void *arg)
{
}

void (*sendData)(int fd, int events, void *arg)
{
}

#endif // REACTOR_H