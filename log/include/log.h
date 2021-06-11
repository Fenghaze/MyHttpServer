/**
 * @author: fenghaze
 * @date: 2021/06/10 09:03
 * @desc: 
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <time.h>
#include <string.h>
class Logger;

//日志的一些属性
class LogEvent
{
public:
    typedef std::shared_ptr<LogEvent> ptr; //指向LogEvent的智能指针

public:
    LogEvent(const char *file, int32_t line, uint32_t elapse,
             uint32_t tid, time_t time);

public:
    const char *getFile() const { return m_file; }
    int32_t getLine() const { return m_line; }
    uint32_t getElapse() const { return m_elapse; }
    uint32_t getThreadId() const { return m_tid; }
    std::string getContent() const { return m_ss.str(); }
    std::stringstream &getSS() { return m_ss; }
    time_t getTime() const { return m_time; }
    std::shared_ptr<Logger> getLogger() const { return m_logger; }

private:
    const char *m_file = nullptr;     //文件名
    int32_t m_line = 0;               //行号
    uint32_t m_elapse = 0;            //程度启动开始到现在的毫秒数
    uint32_t m_tid;                   //线程id
    std::stringstream m_ss;           //消息
    uint64_t m_time = 0;              //时间
    std::shared_ptr<Logger> m_logger; //日志器
};

//日志级别
class LogLevel
{
public:
    enum Level
    {
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    static const char *toString(LogLevel::Level level)
    {
        switch (level)
        {
#define XX(name)      \
    case name:        \
        return #name; \
        break;

            XX(DEBUG);
            XX(INFO);
            XX(WARN);
            XX(ERROR);
            XX(FATAL);
#undef XX
        default:
            return "UNKNOWN";
        }
        return "UNKNOWN";
    }
};

//日志格式器
class LogFormatter
{
public:
    typedef std::shared_ptr<LogFormatter> ptr; //指向LogFormatter的智能指针

    // 时间%t 线程号%tid 消息%m 换行%n
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

public:
    LogFormatter(const std::string &pattern);

    //不同的输出格式，基类
    class FormatItem
    {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        FormatItem(const std::string &format = "") {}
        virtual ~FormatItem() {}
        virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

private:
    //解析m_pattern日志的格式
    void init();
    bool isError() const { return m_error; }

private:
    std::string m_pattern; //日志语句
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};

//日志输出目的地，基类
class LogAppender
{
    friend class Logger;

public:
    typedef std::shared_ptr<LogAppender> ptr; //指向LogAppender的智能指针

public:
    virtual ~LogAppender() {} //定义为虚函数，可以让其他类继承

public:
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    void setFomatter(LogFormatter::ptr formatter) { m_formatter = formatter; }
    LogFormatter::ptr getFormatter() const { return m_formatter; }

protected:
    LogLevel::Level m_level;       //日志级别
    LogFormatter::ptr m_formatter; //输出格式
};

//输出到控制台
class StdoutLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

public:
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
};

//输出到文件
class FileLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string &filename);

public:
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    bool reopen();

private:
    std::string m_filename;
    std::ofstream m_filestream;
};

//日志输出器
class Logger : public std::enable_shared_from_this<Logger>
{
public:
    typedef std::shared_ptr<Logger> ptr; //指向Logger的智能指针

public:
    Logger(const std::string &name = "root");

public:
    void log(LogLevel::Level level, LogEvent::ptr event);

    //debug日志
    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    //添加输出地
    void addAppender(LogAppender::ptr appender);
    //移除输出地
    void delAppender(LogAppender::ptr appender);

    //获取日志级别
    LogLevel::Level getLevel() const { return m_level; }
    //设置日志级别
    void setLevel(LogLevel::Level level) { m_level = level; }

    //获取日志名
    const std::string &getName() const { return m_name; }

private:
    std::string m_name;                      //日志名称
    LogLevel::Level m_level;                 //日志级别
    std::list<LogAppender::ptr> m_appenders; //Appender集合
    LogFormatter::ptr m_fomatter;            //格式
};

#endif // LOG_H