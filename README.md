<center>
	<h1>
        MyHttpServer
    </h1>
</center>

[TOC]

# 🎤前言

一直以来，我认为服务器开发是一件非常酷:sunglasses:的事情，从网上了解到C++后台服务器开发涉及到了许多知识，开发人员不仅需要熟练使用C++，懂得C++的内存管理机制，还要了解TCP/IP原理，此外，还要熟悉Linux下的编程环境，GDB调试，Makefile的编写等，服务器多并发也是很重要的一点，如何设计一个资源消耗少、响应速度快的高性能服务器是后台服务器开发的最终目标。

经过近一年断断续续的学习，上面涉及到的基础知识都一一做了笔记，为了加深理解，搭建了一个基于Linux C++的HttpServer。



# 🔧快速开始





# :tada:介绍

**HttpServer通常包含以下三大任务：**

:small_blue_diamond:建立服务端与客户端的连接

:small_blue_diamond:解析HTTP的request请求

:small_blue_diamond:构造response数据响应HTTP

**为了保证Linux服务器程序的规范性，还涉及到以下应用：**

:small_orange_diamond:连接定时器：关闭非活动连接

:small_orange_diamond:日志系统：记录服务器运行状态

:small_orange_diamond:压力测试：​测试服务器并发连接及数据交互的性能

**本项目涉及到的技术：**

通过**线程池 + epoll模型 + Reactor机制**实现的**并发模型框架**，如图1.所示

使用**有限状态机**解析HTTP的**GET**和**POST**请求，==解析json格式数据==

创建一个简单的Mysql数据库，实现Web用户注册、登录、==注销==的功能，可以请求服务器的图片和视频文件，可以==上传图片、视频到服务器==

==使用Redis辅助缓存==

==用户登录后，可以使用在线版的聊天室==



# :question:遇到的问题

- 代码冗长、不易阅读：使用代码解析工具查看代码结构，能够清楚地看到各个函数之间的依赖关系
- 调试Debug，编写测试代码
- 监控服务器程序的相关命令：netstat、



# :bulb:存在的缺陷





# :star:收获





# :book:参考文献

