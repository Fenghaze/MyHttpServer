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
    buffers_.reserve(16); //扩充容量
}

void AsyncLogger::start()
{
    running_ = true;
    //创建线程并运行
    thread_ = std::thread([this]()
                          { this->threadFunc(); });
}

void AsyncLogger::stop()
{
    running_ = false;
    //唤醒等待的条件变量
    cond_.notify_one();
    //回收线程
    thread_.join();
}

/******************************************************************** 
Description : 
前端在生成一条日志消息时，会调用AsyncLogging::append()。
如果currentBuffer_够用，就把日志内容写入到currentBuffer_中，
如果不够用(就认为其满了)，就把currentBuffer_放到buffer_数组中，
等待消费者线程（即后台线程）来取。则将预备好的另一块缓冲
（nextBuffer_）移用为当前缓冲区（currentBuffer_）。
*********************************************************************/
void AsyncLogger::append(const char *logline, size_t len)
{
    MutexLockGuard lock(mutex_); //保护缓冲区和缓冲队列
    // 如果当前buffer的长度大于要添加的日志记录的长度，即当前buffer还有空间，就添加到当前日志。
    if (currentBuffer_->avail() > len)
    {
        currentBuffer_->append(logline, len);
    }
    // 当前buffer已满，使用另外的缓冲区
    else
    {
        //1、先将currentBuffer_保存到缓冲区队列中
        buffers_.push_back(std::move(currentBuffer_));
        //2、如果另一块缓冲区不为空，则将预备好的另一块缓冲区移用为当前缓冲区
        if (nextBuffer_)
        {
            currentBuffer_ = std::move(nextBuffer_);
        }
        //如果前端写入速度太快了，一下子把两块缓冲都用完了，那么只好分配一块新的buffer，作当前缓冲区
        else
        {
            currentBuffer_.reset(new Buffer); // Rarely happens
        }
        //3、添加日志记录
        currentBuffer_->append(logline, len);
        //4、通知后端开始写入日志数据
        cond_.notify_one();
    }
}

/******************************************************************** 
Description : 
如果buffers_为空，使用条件变量等待条件满足（即前端线程把一个已经满了
的buffer放到了buffers_中或者超时）。将当前缓冲区放到buffers_数组中。
更新当前缓冲区（currentBuffer_）和另一个缓冲区（nextBuffer_）。
将bufferToWrite和buffers_进行swap。这就完成了将写了日志记录的buffer
从前端线程到后端线程的转变。
*********************************************************************/
void AsyncLogger::threadFunc()
{
    assert(running_ == true);
    //定义两个Buffer指针
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    // 写入日志记录文件的缓冲队列
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    //打开日志文件流
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
            // 如果buffers_为空，那么表示没有日志数据需要写入文件，那么就等待指定的时间
            if (buffers_.empty()) // unusual usage!
            {
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }
            // 无论cond是因何（一是超时，二是当前缓冲区写满了）而醒来，都要将currentBuffer_放到buffers_中。
            // 如果是因为时间到（3秒）而醒，那么currentBuffer_还没满，此时也要将之写入LogFile中。
            // 如果已经有一个前端buffer满了，那么在前端线程中就已经把一个前端buffer放到buffers_中了
            // 此时，还是需要把currentBuffer_放到buffers_中（注意，前后放置是不同的buffer，
            // 因为在前端线程中，currentBuffer_已经被换成nextBuffer_指向的buffer了）。
            buffers_.push_back(std::move(currentBuffer_));
            // 将新的buffer（newBuffer1）移用为当前缓冲区（currentBuffer_）
            currentBuffer_ = std::move(newBuffer1);
            // buffers_和buffersToWrite交换数据
            // 此时buffers_所有的数据存放在buffersToWrite，而buffers_变为空
            buffersToWrite.swap(buffers_);
            // 如果nextBuffer_为空，将新的buffer（newBuffer2）移用为另一个缓冲区（nextBuffer_）
            if (!nextBuffer_)
            {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        assert(!buffersToWrite.empty());

        // 如果将要写入文件的buffer列表中buffer的个数大于25，那么将多余数据删除。
        // 前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这是典型的生产速度超过消费速度，
        // 会造成数据在内存中的堆积，严重时引发性能问题(可用内存不足)或程序崩溃(分配内存失败)。
        if (buffersToWrite.size() > 25)
        {
            // TODO::dropping log messages
            printf("two much\n");
        }

        // 将buffersToWrite的数据写入到日志文件中
        for (size_t i = 0; i < buffersToWrite.size(); ++i)
        {
            fwrite(buffersToWrite[i]->data(), 1, buffersToWrite[i]->size(), stream);
        }

        // 重新调整buffersToWrite的大小
        if (buffersToWrite.size() > 2)
        {
            buffersToWrite.resize(2);
        }

        // 从buffersToWrite中弹出一个作为newBuffer1 
        if (!newBuffer1)
        {
            assert(!buffersToWrite.empty());
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        // 从buffersToWrite中弹出一个作为newBuffer2
        if (!newBuffer2)
        {
            assert(!buffersToWrite.empty());
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        // 清空buffersToWrite
        buffersToWrite.clear();
        fflush(stream);
    }
    fflush(stream);
}
