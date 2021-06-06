/**
 * @author: fenghaze
 * @date: 2021/06/05 14:51
 * @desc: 
 */

#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <signal.h>
#include <string.h>



//将fd设置为非阻塞
static int setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

//添加节点到epfd
static void addfd(int epfd, int fd, bool et = true)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLERR;
    if (et)
    {
        ev.events |= EPOLLET;
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

//从epfd删除节点
static void delfd(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//修改epfd的节点
// static void modfd(int epfd, int fd, epoll_event event)
// {
//     epoll_event ev;
//     ev.data.fd = fd;
//     ev.events = event;
//     epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
// }

#endif // UTILS_H