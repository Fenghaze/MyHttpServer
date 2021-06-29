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

- `thread logThread`：异步日志线程

- `pid_t tid`：当前运行的线程号

- `bool _runing`：是否运行线程

- `BlockingQueue<std::string> queue`：阻塞队列

- ```c++
  //重载LogStream的<<运算符，内部调用s.append()保存数据
  LogStream &operator<<(LogStream &s, T v);	//T是一个字符串类
  LogStream &operator<<(LogStream &s, Logger::SourceFile file);
  ```

- `string g_logFileName`：日志文件名

- `LogLevel g_logLevel`：日志级别，初始化为`INFO`

- `OutputFunc g_output`：输出的回调函数，初始化为`defaultOutput`

- `ofstream logStream`：文件流对象，打开的是`g_logFileName`

## BlockingQueue





## AsyncLogger

AsyncLogger.h/cpp 异步日志对象

# 优化

一开始的那个使用BlockingQueue加STL库string的方法，测试写入效率只有30M/s，效率太低。
改用char数组封装缓冲区加上双缓存的方法，写入效率可达60M/s。

