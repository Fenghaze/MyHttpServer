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
    //添加日志
    void append(const char *logline, size_t len);

  private:
    //线程函数
    void threadFunc();

    using MutexLockGuard = std::lock_guard<std::mutex>;

    using Buffer = detail::FixBuffer<detail::kLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;
    //定义std::unique_ptr<Buffer>为BufferPtr指针
    using BufferPtr = BufferVector::value_type;

    const int flushInterval_; //刷新缓冲区的时间间隔
    bool running_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    BufferPtr currentBuffer_; //vec中指向当前buffer的指针
    BufferPtr nextBuffer_;    //vec中指向下一个buffer的指针
    BufferVector buffers_;    //存放buffer指针的vec
  };

}

#endif // CLOG_ASYNCLOGGER_H
