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

using namespace clog;

class TestLog
{
public:
    TestLog(int printtimes = 3) : times(printtimes)
    {
        for (int i = 0; i < times; i++)
        {
            LOG_TRACE << "TestLog test trace-----" << i + 1;
        }
    }
    ~TestLog()
    {
        LOG_TRACE << "~TestLog()";
    }

private:
    int times;
};

#endif // TESTLOG_H