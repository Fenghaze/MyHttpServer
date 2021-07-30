/*
*   异步日志器：生成异步日志对象
*   使用阻塞队列来传递日志消息
*/
#ifndef CLOG_ASYNCLOGGER_H
#define CLOG_ASYNCLOGGER_H

#include "LogStream.h"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <memory>

namespace clog
{

  class AsyncLogger
  {
  public:
    //构造函数初始化变量
    AsyncLogger(int flushInterval = 3);

    ~AsyncLogger()
    {
      if (running_)
        stop();
    }

    //运行异步日志线程
    void start();
    //停止运行
    void stop();
    //添加日志，前端在生成一条日志消息时，会调用AsyncLogging::append()
    void append(const char *logline, size_t len);

  private:
    //线程函数
    void threadFunc();

    using MutexLockGuard = std::lock_guard<std::mutex>;

    using Buffer = detail::FixBuffer<detail::kLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;
    //定义std::unique_ptr<Buffer>为BufferPtr指针
    using BufferPtr = BufferVector::value_type;

    const int flushInterval_; // 定期（flushInterval_秒）将缓冲区的数据写到文件中
    bool running_;            // 运行写日志线程
    std::thread thread_;      // 日志线程
    std::mutex mutex_;
    std::condition_variable cond_;
    //双缓冲技术
    //准备2个BufferA、B：前端负责保存数据到Buffer A，后端负责将Buffer B中的数据写入文件
    //当Buffer A写满之后，和B交换，此时前端负责操作Buffer B，后端负责操作Buffer A
    //缓冲区队列保存了多个前端的Buffer，可以将多个日志信息拼接成一个大的Buffer传入给后端，减少线程唤醒的开销
    BufferPtr currentBuffer_;   //当前缓冲区
    BufferPtr nextBuffer_;    //下一个缓冲区
    BufferVector buffers_;    //缓冲区队列
  };

}

#endif // CLOG_ASYNCLOGGER_H
