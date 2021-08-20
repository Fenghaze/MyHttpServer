/**
 * @author: fenghaze
 * @date: 2021/07/15 11:37
 * @desc: 在类中使用异步日志
 */

#ifndef TESTLOG_H
#define TESTLOG_H
#include "Logger.h"
#include "AsyncLogger.h"
#include "Localtime.h"
#include <iostream>
#include <thread>
#include <unistd.h>
#include <stdio.h>
#include <thread>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace clog;

class TestLog
{
public:
    TestLog(int printtimes = 3) : times(printtimes)
    {
        flag = false;
        LOG_INFO << "testlog thread....";
        std::thread tid(run);
        tid.join();
    }
    ~TestLog()
    {
        LOG_TRACE << "TestLog() delete";
    }

    static void run()
    {
        for (size_t i = 0; i < 3; i++)
        {
            LOG_WARN << "worker thread...";
        }
    }

private:
    bool flag;
    int times;
};

#endif // TESTLOG_H