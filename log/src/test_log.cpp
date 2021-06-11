#include "../include/log.h"
#include <iostream>

int main(int argc, char const *argv[])
{
    Logger::ptr logger(new Logger);
    logger->addAppender(LogAppender::ptr(new StdoutLogAppender));
    LogEvent::ptr event(new LogEvent(__FILE__, __LINE__, 0, 1, time(0)));
    event->getSS() << "hello world";

    logger->log(LogLevel::DEBUG, event);
    std::cout << "hello log 2" << std::endl;

    return 0;
}
