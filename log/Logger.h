/*
*   日志器：生成日志对象
*   单线程下，把日志记录写到缓冲区中
*   setConcurrentMode启动异步日志模式：专门开了一个线程来记录日志，使用阻塞队列来传递日志消息
*/
#ifndef CLOG_LOGGER_H
#define CLOG_LOGGER_H

#include "LogStream.h"
#include "AsyncLogger.h"
#include <memory>

#include <string.h>

namespace clog
{

    class Logger
    {
    public:
        //日志级别
        enum LogLevel
        {
            TRACE,
            DEBUG,
            INFO,
            WARN,
            ERROR,
            FATAL,
            LogLevelNum
        };

        //日志文件：有两个属性（文件名，文件名长度）
        class SourceFile
        {
        public:
            template <int N>
            inline SourceFile(const char (&arr)[N])
                : _data(arr),
                  _size(N - 1)
            {
                const char *slash = strrchr(_data, '/');    //strrchr查找最后一次出现'/'之后的字符串
                if (slash)
                {
                    _data = slash + 1;
                }
                _size = strlen(_data);
            }

            explicit SourceFile(const char *filename)
                : _data(filename)
            {
                const char *slash = strrchr(_data, '/');
                if (slash)
                {
                    _data = slash + 1;
                }
                _size = strlen(_data);
            }

            const char *_data;
            size_t _size;
        };

        //构造函数：实例化Impl
        Logger(SourceFile file, int line);
        Logger(SourceFile file, int line, LogLevel level);
        Logger(SourceFile file, int line, LogLevel level, const char *func);
        Logger(SourceFile file, int line, bool toAbort);
        //析构函数：打印最后一行日志、异步日志输出日志到文件
        ~Logger();

        //获取日志流对象
        LogStream &stream() noexcept;
        //获取日志级别
        static LogLevel logLevel() noexcept;
        //设置日志级别
        static void setLogLevel(LogLevel level) noexcept;

        //启动异步模式
        static void setConcurrentMode();

        //static void finishConcurrent();

        //void (*)(const std::string &)函数指针取别名为OutputFunc
        using OutputFunc = void (*)(const std::string &);
        //设置输出的回调函数
        static void setOutput(OutputFunc) noexcept;
    
    public:

    private:
        //Impl类：实现日志输出
        class Impl;
        std::unique_ptr<Impl> _pImpl;   
    };

} // end of namespace clog

//宏定义
#define LOG_TRACE                                        \
    if (clog::Logger::logLevel() <= clog::Logger::TRACE) \
    clog::Logger(__FILE__, __LINE__, clog::Logger::TRACE, __func__).stream()

#define LOG_DEBUG                                        \
    if (clog::Logger::logLevel() <= clog::Logger::DEBUG) \
    clog::Logger(__FILE__, __LINE__, clog::Logger::DEBUG, __func__).stream()

#define LOG_INFO                                        \
    if (clog::Logger::logLevel() <= clog::Logger::INFO) \
    clog::Logger(__FILE__, __LINE__, clog::Logger::INFO, __func__).stream()

#define LOG_WARN                                        \
    if (clog::Logger::logLevel() <= clog::Logger::WARN) \
    clog::Logger(__FILE__, __LINE__, clog::Logger::WARN, __func__).stream()

#define LOG_ERROR                                        \
    if (clog::Logger::logLevel() <= clog::Logger::ERROR) \
    clog::Logger(__FILE__, __LINE__, clog::Logger::ERROR, __func__).stream()

#define LOG_FATAL clog::Logger(__FILE__, __LINE__, clog::Logger::FATAL, __func__).stream()

#define LOG_SYSERR clog::Logger(__FILE__, __LINE__, false).stream()

#define LOG_SYSFATAL clog::Logger(__FILE__, __LINE__, true).stream()

#endif // CLOG_LOGGER_H
