#include "AsyncLogger.h"

#include <assert.h>
#include <stdio.h>

#include <iostream>

using namespace clog;

AsyncLogger::AsyncLogger(int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      thread_(),
      mutex_(),
      cond_(),
      currentBuffer_(new Buffer),
      nextBuffer_(new Buffer),
      buffers_()
{
    buffers_.reserve(16);   //扩充容量
}

void AsyncLogger::start()
{
    running_ = true;
    //创建线程并运行
    thread_ = std::thread([this](){ this->threadFunc(); });
}

void AsyncLogger::stop()
{
    running_ = false;
    //唤醒等待的条件变量
    cond_.notify_one();
    //回收线程
    thread_.join();
}

//异步互斥，增加buffer长度和内容
void AsyncLogger::append(const char *logline, size_t len)
{
    MutexLockGuard lock(mutex_);
    //空间足够，直接增加
    if (currentBuffer_->avail() > len)
    {
        currentBuffer_->append(logline, len);
    }
    //空间不足，使用双缓存
    else
    {
        //1、先将currentBuffer_保存到vec中
        buffers_.push_back(std::move(currentBuffer_));
        //2、如果有next，则cur指向next
        if (nextBuffer_)
        {
            currentBuffer_ = std::move(nextBuffer_);
        }
        //否则清空cur
        else
        {
            currentBuffer_.reset(new Buffer); // Rarely happens
        }
        //3、新的cur追加内容
        currentBuffer_->append(logline, len);
        //唤醒等待的条件变量
        cond_.notify_one();
    }
}

void AsyncLogger::threadFunc()
{
    assert(running_ == true);
    //定义两个Buffer指针
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    //定义Buffer指针数组
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    //打开日志文件流，可写
    FILE *stream = fopen(detail::getLogFileName().data(), "w");
    assert(stream);

    while (running_)
    {
        assert(newBuffer1 && newBuffer1->size() == 0);
        assert(newBuffer2 && newBuffer2->size() == 0);
        assert(buffersToWrite.empty());

        {
            //写缓冲区时，需要加锁
            std::unique_lock<std::mutex> lock(mutex_);
            if (buffers_.empty()) // unusual usage!
            {
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }
            //vec保存当前buffer指针
            buffers_.push_back(std::move(currentBuffer_));
            //当前buffer指向新的buffer指针1
            currentBuffer_ = std::move(newBuffer1);
            //交换vec
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_)
            {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        if (buffersToWrite.size() > 25)
        {
            // TODO::dropping log messages
            printf("two much\n");
        }

        //二进制流写入数据
        for (size_t i = 0; i < buffersToWrite.size(); ++i)
        {
            fwrite(buffersToWrite[i]->data(), 1, buffersToWrite[i]->size(), stream);
        }

        if (buffersToWrite.size() > 2)
        {
            buffersToWrite.resize(2);
        }

        if (!newBuffer1)
        {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2)
        {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        fflush(stream);
    }
    fflush(stream);
}
