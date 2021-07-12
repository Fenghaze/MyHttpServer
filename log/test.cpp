#include "Logger.h"
#include "AsyncLogger.h"
#include "Localtime.h"

#include <iostream>
#include <thread>
#include <unistd.h>
#include <stdio.h>

using namespace std;

void tf()
{
    for (int i = 0; i < 1; ++i)
    {
        LOG_TRACE << "test TRACE----------------------------";
        LOG_DEBUG << "test DEBUG----------------------------";
        LOG_INFO << "test INFO----------------------------------";
        LOG_WARN << "test WARN --------------------------------";
        LOG_ERROR << "test xxxx <<<<<<<<<<< -------";
    }
}

int main()
{
    using namespace clog;
    Logger::setLogLevel(Logger::TRACE);
    Logger::setConcurrentMode();
    Localtime begin(Localtime::now());

    thread t(tf);
    // thread t2(tf);
    for (int i = 0; i < 1; ++i)
    {
        //Logger构造函数宏
        LOG_TRACE << "test TRACE----------------------------";
        LOG_DEBUG << "test DEBUG----------------------------";
        LOG_INFO << "test INFO----------------------------------";
        LOG_WARN << "test WARN --------------------------------";
        LOG_ERROR << "test xxxx <<<<<<<<<<< -------";
    }

    t.join();
    // t2.join();

    double times = timeDifference(Localtime::now(), begin);

    printf("Time is %10.4lf s\n", times);

    return 0;
}

// clog::AsyncLogger logger;

// string str1("1111\ttest lohg-----------------------sfifhauofghauofgafaffa----------da1\n");
// string str2("2222\tgfduadifgadsiuyfgasiyfgfahufaogfoaasiyfgasiyfasiyfasyidfasiyfdaisy2\n");
// string str3("3333\tjipyhgo8yhiayvfauftasuvsijfgeujfagsuofgauofaguoffgahkfgaiy6fgahikdfbvahk3\n");
// string str4("444\tfhienrikqpfnianfipqwfjqiopfmcalm,cla;kcaopfafafhasufhauogatqw4\n");
// string str5("555\tfahuofajfiqpknrfkemngkltymnhioyjp[okgopsxjgpuejropghfabfuaguyfjitr5\n");

// void tf1()
// {

//     for (int i = 0; i < 5; ++i)
//     {
//         logger.append(str5.data(), str5.size());
//         logger.append(str1.data(), str1.size());
//         logger.append(str3.data(), str3.size());
//         logger.append(str4.data(), str4.size());
//         logger.append(str2.data(), str2.size());
//     }
// }

// void tf2()
// {

//     for (int i = 0; i < 5; ++i)
//     {
//         logger.append(str1.data(), str1.size());
//         logger.append(str3.data(), str3.size());
//         logger.append(str4.data(), str4.size());
//         logger.append(str5.data(), str5.size());
//         logger.append(str2.data(), str2.size());
//     }
// }

// void tf3()
// {

//     for (int i = 0; i < 5; ++i)
//     {
//         logger.append(str1.data(), str1.size());
//         logger.append(str4.data(), str4.size());
//         logger.append(str5.data(), str5.size());
//         logger.append(str3.data(), str3.size());
//         logger.append(str2.data(), str2.size());
//     }
// }

// int main()
// {
//     using namespace clog;

//     logger.start();

//     Localtime begin(Localtime::now());

//     thread t1(tf1);
//     thread t2(tf2);
//     thread t3(tf3);

//     for (int i = 0; i < 10; ++i)
//     {
//         logger.append(str2.data(), str2.size());
//         logger.append(str4.data(), str4.size());
//         logger.append(str1.data(), str1.size());
//         logger.append(str5.data(), str5.size());
//         logger.append(str3.data(), str3.size());
//     }

//     t1.join();
//     t2.join();
//     t3.join();

//     double times = timeDifference(Localtime::now(), begin);

//     printf("Time is %10.4lf s\n", times);

//     return 0;
// }
