<center>
	<h1>
MyHttpServer
</h1>
</center>
- [前言](#head1)
- [快速开始](#head2)
- [介绍](#head3)
  - [项目结构](#head4)
- [遇到的问题](#head5)
- [可改进的地方](#head6)
- [收获](#head7)
- [参考文献](#head8)


# <span id="head1"> 前言</span>

C++后台服务器开发涉及到了许多知识，开发人员不仅需要熟练使用C++，懂得C++的内存管理机制，还要了解TCP/IP原理，此外，还要熟悉Linux下的编程环境，GDB调试，Makefile的编写等，服务器多并发也是很重要的一点，如何设计一个资源消耗少、响应速度快的高性能服务器是后台服务器开发的最终目标。

经过近一年时间断断续续的学习，上面涉及到的基础知识都一一做了笔记，为了加深理解，实现一个基于Linux C++的HttpServer。



# <span id="head2">快速开始</span>





# <span id="head3">介绍</span>

**HttpServer通常包含以下三大任务：**

:small_blue_diamond:建立服务端与客户端（浏览器）的TCP连接

:small_blue_diamond:解析HTTP的请求

:small_blue_diamond:响应HTTP，返回html代码或其他资源文件给客户端

**为了保证Linux服务器程序的规范性，还涉及到以下应用：**

:small_orange_diamond:定时器：设置一些定时检查事件

:small_orange_diamond:日志系统：记录服务器运行状态

:small_orange_diamond:压力测试：测试服务器并发连接及数据交互的性能



**根据上述介绍，本项目涉及到如下技术：**

:white_square_button:实现两种经典的并发模型：

- [x] **epoll+进程池+reactor**，master进程负责监听lfd并派发任务给worker进程，worker进程负责accept
- [ ] **epoll+线程池+reactor**，main线程负责accept，worker线程负责处理IO

:white_square_button:HTTP服务类的实现：

- [ ] 使用**有限状态机**解析HTTP的**GET**和**POST**请求；
- [ ] 实现**大文件传输**

:white_square_button:定时器类的实现：

- [x] 实现了基于链表、时间轮和最小堆的三种定时器容器，用于处理**非活动连接**

:white_square_button:日志系统：

- [x] 双缓存异步日志系统

:white_square_button:压力测试：

- [ ] 编写了一个多线程客户端，每个线程负责发送N个客户连接请求，main线程负责监听cfd，不断发送GET请求

- [ ] 使用压力测试工具http_load



## <span id="head4">项目结构</span>

```shell
.
├── clock
│   ├── listClock.h
│   ├── test_heap.cpp
│   ├── timeHeap.h
│   └── timeWheel.h
├── http
│   ├── http.cpp
│   └── http.h
├── lock
│   ├── lock.h
│   └── README.md
├── main.cpp
├── one_loop_per_thread
├── processpool
│   ├── http.cpp
│   ├── http.h
│   ├── HTTPServer.cpp
│   ├── locker.h
│   ├── makefile
│   ├── processpool.h
│   └── Server
├── README.md
├── test
│   ├── test
│   └── test.cpp
├── threadpool
│   └── threadpool.h
└── utils
    ├── reactor.h
    └── utils.h
```



# <span id="head5">遇到的问题</span>

- 调试多线程/多进程的Debug，编写测试代码，ps命令的使用
- 代码冗长、不易阅读：使用代码解析工具查看代码结构，能够清楚地看到各个函数之间的依赖关系
- 监控服务器程序的相关命令：netstat、
- 短连接TIME_WAIT的处理方式
- C++内存泄漏



# <span id="head6">可改进的地方</span>





# <span id="head7">收获</span>





# <span id="head8">参考文献</span>