/**
 * @author: fenghaze
 * @date: 2021/06/02 10:58
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
#include <string>
#include "efficient_threadpool.h"

#define SERVERPORT "12345"

/*用于处理客户CGI请求的类，它可以作为processpool类的模板参数*/
class CGIConn
{
public:
    CGIConn() {}
    ~CGIConn() {}
    void init(int epfd, int sockfd, const sockaddr_in &raddr);
    void process();

private:
    static const int BUFFER_SIZE = 1024; //读缓冲区大小
    static int m_epollfd;                //epoll句柄
    int m_sockfd;                        //通信的fd
    sockaddr_in m_addr;                  //socket地址
    char m_buf[BUFFER_SIZE];             //读缓冲区
    int m_read_idx;                      //标记缓冲区中已读入的客户数据的最后一个字节的下一个位置
};
//初始化静态类成员变量
int CGIConn::m_epollfd = -1;

//初始化客户连接，清空读缓冲区
void CGIConn::init(int epfd, int sockfd, const sockaddr_in &raddr)
{
    m_epollfd = epfd;
    m_sockfd = sockfd;
    m_addr = raddr;
    memset(m_buf, '\0', BUFFER_SIZE);
    m_read_idx = 0;
}

//处理客户数据
void CGIConn::process()
{
    int idx = 0;
    int ret = -1;
    while (true)
    {
        idx = m_read_idx; //记录数据开始地址
        //接收数据时，读缓冲区大小逐渐减小
        ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
        if (ret < 0)
        {
            if (errno != EAGAIN)
            {
                removefd(m_epollfd, m_sockfd);
            }
            break;
        }
        else
        {
            m_read_idx += ret; //读取到的数据长度
            printf("user content is: %s\n", m_buf);
            //如果遇到字符 "\r\n" 回车+换行，则处理客户请求
            for (; idx < m_read_idx; idx++)
            {
                if ((idx >= 1) && (m_buf[idx - 1] == '\r') && (m_buf[idx] == '\n'))
                {
                    break;
                }
            }
            //如果没有遇到字符“\r\n”，则需要读取更多客户数据
            if (idx == m_read_idx)
            {
                continue;
            }
            m_buf[idx - 1] = '\0';
            char *file_name = m_buf;
            // 判断客户要运行的CGI程序是否存在
            if (access(file_name, F_OK) == -1)
            {
                removefd(m_epollfd, m_sockfd);
                break;
            }
            //创建子进程运行CGI程序
            int pid = fork();
            if (pid == -1)
            {
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else if (pid == 0) //运行CGI服务
            {
                close(STDOUT_FILENO);   //关闭标准输出流
                dup(m_sockfd);          //m_sockfd引用+1,写到stdout的数据重定向到m_sockfd中
                execl(m_buf, m_buf, 0); //
                exit(0);
            }
            else
            {
                removefd(m_epollfd, m_sockfd);
                break;
            }
        }
    }
};


int main(int argc, char const *argv[])
{
    int lfd;
    struct sockaddr_in laddr;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(SERVERPORT));
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);

    bind(lfd, (struct sockaddr *)&laddr, sizeof(laddr));

    listen(lfd, 5);

    //创建CGI服务进程池
    //从线程池中分离一个主线程，这个线程的任务是负责监听lfd，并派发任务给线程池中的子线程
    //从线程池类中获得一个子线程，传递cfd给这个子线程
    //这个子线程创建epoll，监听cfd和信号
    EfficientThreadPool<CGIConn> &threadpool = EfficientThreadPool<CGIConn>::create(lfd, 5, 10);

    return 0;
}