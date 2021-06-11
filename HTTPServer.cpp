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
#include "processpool.h"
#include "http.h"
#define SERVERPORT "7777"
#define USE_PROCESSPOOL

int main(int argc, char const *argv[])
{
    int lfd;
    struct sockaddr_in laddr;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(atoi(SERVERPORT));
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);

    int val = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    bind(lfd, (struct sockaddr *)&laddr, sizeof(laddr));

    listen(lfd, 128);

#ifdef USE_PROCESSPOOL
    ProcessPool<HTTPConn> &pool = ProcessPool<HTTPConn>::create(lfd, 5, 10);
    pool.run();
    close(lfd);
#endif // USE_PROCESSPOOL

    return 0;
}