# 日志系统
# 编译运行

```shell
mkdir build
cd build
cmake ..
make
```

# 用法
在单线程下直接`LOG_XXXX << "log entry"`。
在多线程下需要先`Logger::setConcurrentMode()`，并且在程序结束时调用`Logger::finishConcurrent()`,不过应该有更好到办法，想到再来修改。

# 模块
## LogStream

**实现功能：**

LogStream.h实现了两个类：

- `class FixBuffer<SIZE>`：日志缓冲区，成员变量和主要成员函数如下

  - `char data_[SIZE]`：缓冲区
  - `char *cur_`：缓冲区指针
  - `void append(const char *data, size_t len)`：将数据复制到cur_中，并增加len个长度
  - `void add(size_t len)`：将cur_指针移动len个长度
  - `void reset()`：`data_`赋值给`cur_`

- `class LogStream`：**流对象**，成员变量和主要成员函数如下

  - `FixBuffer<4096> _buffer`：日志缓冲区对象
  - `LogStream &operator<<(arg)`：重载`<<`运算符，括号内传入的参数是各类型数据，**与`std::cout<<`输出到控制台不同，`LogStream`对象使用`<<`后，会把数据保存到`_buffer`中**

  - `void append(const char *data, size_t len)`：内部调用`_buffer.append(data, len)`

## Localtime

**实现功能：**

Localtime.h实现了一个类、重载比较运算符、增加时间、计算时间差：

- `class Localtime`：时间戳类，单位为微秒，成员变量和主要成员函数如下
  - `long long _microSecondsSinceEpoch`：微秒数
  - `static Localtime now()`：使用`gettimeofday()`获取当前时间，并转换为微秒数
  - `string toFormattedString(bool showMicroSeconds = true)`：，使用`localtime_r`**格式化**微秒数
- `bool operator<(Localtime lhs, Localtime rhs)`：重载<运算符，比较两个时间的大小，还实现了其他比较运算符，如`>, ==, >=, <=`
- `Localtime addTime(const Localtime localtime, double seconds)`：在原有的时间上加上秒数得到新的时间
- `double timeDifference(Localtime time1, Localtime time2)`：求两个时间到差值，返回秒数

## Logger

**实现功能：**

Logger.h实现了三个类、重载`LogStream`的`<<`运算符和日志类对象的宏定义：

- `class Logger`：日志类，成员变量和主要成员函数如下

  - `enum LogLevel`：定义6个日志级别（TRACE、DEBUG、INFO、WARN、ERROR、FATAL）

  - `unique_ptr<Impl> _pImpl`：`Impl`对象指针

  - `class SourceFile`：日志文件类，负责管理两个属性（文件名，文件名长度）

  - `class Impl`：日志具体操作类，成员变量和主要成员函数如下

    - ```c++
      LogLevel _level;			//日志级别
      Logger::SourceFile _file;	//日志文件对象
      int _line;					//日志所在行
      Localtime _time;			//本地时间
      LogStream _stream;    		//日志流对象
      ```

    - ```c++
      //构造函数：初始化成员函数，日志流对象输出当前线程号、格式化的时间戳、日志级别到日志文件
      //日志输出就是在这个构造函数中完成的
      Impl(LogLevel level, int savedErrno, const Logger::SourceFile &file, int line);
      ```
  
  - ```c++
    //构造函数：每个构造函数传入不同的参数，实例化一个Impl对象
    Logger(SourceFile file, int line);	//文件名、行号
    Logger(SourceFile file, int line, LogLevel level);	//文件名、行号、日志级别
    Logger(SourceFile file, int line, LogLevel level, const char *func);	//文件名、行号、日志级别、函数名
    Logger(SourceFile file, int line, bool toAbort);	//文件名、行号、错误码
    ```
  
  - `static void setConcurrentMode()`：启动异步日志线程

> Logger.cpp：异步日志的初始设置

- `AsyncLogger logger`：异步日志对象

- `pid_t tid`：当前运行的线程号

- ```c++
  //重载LogStream的<<运算符，内部调用s.append()保存数据
  LogStream &operator<<(LogStream &s, T v);	//T是一个字符串类
  LogStream &operator<<(LogStream &s, Logger::SourceFile file);
  ```

- `string g_logFileName`：日志文件名

- `LogLevel g_logLevel`：日志级别，初始化为`INFO`


## AsyncLogger

**实现功能：**

AsyncLogger.h实现了一个类：

- `class AsyncLogger`：双缓冲异步日志类，成员变量和主要成员函数如下

  - ```c++
    std::mutex mutex_;				//阻塞队列互斥锁
    std::condition_variable cond_;	//阻塞队列条件变量
    
    using Buffer = detail::FixBuffer<detail::kLargeBuffer>;		//缓冲区重命名
    using BufferVector = std::vector<std::unique_ptr<Buffer>>;	//vector存放缓冲区指针
    using BufferPtr = BufferVector::value_type;		//vector中的每个元素（指向缓冲区的指针）
    
    std::thread thread_;			//日志线程
    bool running_;					//线程运行标志
    const int flushInterval_; 		//刷新缓冲区的时间间隔
    
    BufferPtr currentBuffer_; 		//vec中指向当前buffer的指针
    BufferPtr nextBuffer_;    		//vec中指向下一个buffer的指针
    BufferVector buffers_;    		//存放buffer指针的vec
    ```

  - `void start()`：创建并运行日志线程

  - `void stop()`：停止运行线程

  - `void append(const char *logline, size_t len)`：添加日志信息

  - `void threadFunc()`：**双缓冲机制实现异步日志**

  ```c++
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
  ```

  

# 优化

一开始的那个使用BlockingQueue加STL库string的方法，测试写入效率只有30M/s，效率太低。
改用char数组封装缓冲区加上双缓存的方法，写入效率可达60M/s。

