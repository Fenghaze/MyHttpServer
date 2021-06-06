/**
 * @author: fenghaze
 * @date: 2021/05/08 19:53
 * @desc: 使用epoll模型实现客户端，对服务器进行压力测试
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENTS_NUM 10000
#define BUFFERSIZE 2048
#define SERVERPORT  "7777"

/*每个客户连接不停地向服务器发送这个请求*/
static const char *request = "GET http://localhost/index.html HTTP/1.1\r\nConnection:keep-alive\r\n\r\nxxxxxxxxxxxx";

int setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

void addfd(int epfd, int fd)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLOUT | EPOLLET | EPOLLERR; //监听可写事件
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

//向服务端ip port发送nums个连接请求
void start_conn(int epfd, int nums, const char *ip, int port)
{
    int ret = 0;
    struct sockaddr_in raddr;
    //配置服务端socket
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &raddr.sin_addr);

    //发送连接请求
    for (size_t i = 0; i < nums; i++)
    {
        usleep(1000);
        //创建与服务端通信的socket
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        printf("create 1 sock\n");
        if (sockfd < 1)
        {
            continue;
        }
        //请求连接，如果连接建立成功，说明可以开始写入数据，监听可写事件
        if (connect(sockfd, (struct sockaddr *)&raddr, sizeof(raddr)) == 0)
        {
            printf("build connection\n", i);
            addfd(epfd, sockfd);
        }
    }
}

//从服务器读取数据
bool read_once(int fd, char *buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(fd, buffer, len, 0);
    if (bytes_read == -1)
    {
        return false;
    }
    else if (bytes_read == 0)
    {
        return false;
    }
    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, fd, buffer);
    return true;
}

bool write_bytes(int fd, const char *buffer, int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, fd);
    while (1)
    {
        bytes_write = send(fd, buffer, len, 0);
        if (bytes_write == -1 || bytes_write == 0)
        {
            return false;
        }
        len -= bytes_write;    //剩余的字符长度
        buffer += bytes_write; //剩余字符串的首地址
        if (len <= 0)
        {
            return true;
        }
    }
}

void close_conn(int epfd, int fd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

int main(int argc, char const *argv[])
{
    int epfd = epoll_create(1);
    //发送连接请求:int nums, const char *ip, int port
    //start_conn(epfd, atoi(argv[3]), argv[1], atoi(argv[2]));
    start_conn(epfd, atoi(argv[1]), "0.0.0.0", atoi(SERVERPORT));

    epoll_event events[MAX_EVENTS_NUM];
    char buffer[BUFFERSIZE];
    //暂停发送接受服务
    pause();
    while (1)
    {
        int fds = epoll_wait(epfd, events, MAX_EVENTS_NUM, 2000);
        for (size_t i = 0; i < fds; i++)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLOUT)
            {
                //向服务器发送请求数据
                if (!write_bytes(sockfd, request, strlen(request)))
                {
                    close_conn(epfd, sockfd);
                }
                epoll_event ev;
                ev.data.fd = sockfd;
                ev.events = EPOLLIN | EPOLLET | EPOLLERR;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if (events[i].events & EPOLLIN)
            {
                //从服务器读取数据
                if (!read_once(sockfd, buffer, BUFFERSIZE))
                {
                    close_conn(epfd, sockfd);
                }
                epoll_event ev;
                ev.data.fd = sockfd;
                ev.events = EPOLLOUT | EPOLLET | EPOLLERR;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if (events[i].events & EPOLLERR)
            {
                close_conn(epfd, sockfd);
            }
        }
    }
    return 0;
}