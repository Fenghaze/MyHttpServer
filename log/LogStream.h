/*
*   FixBuffer类：保存日志数据
*   日志输出流：使用一个std::string作为缓冲区
*   重载 << 输出运算符，将各类型数据保存到buffer中
*/

#ifndef CLOG_LOGSTREAM_H
#define CLOG_LOGSTREAM_H

#include <string.h>

#include <vector>
#include <algorithm>
#include <string>

namespace clog
{

    namespace detail
    {

        const int kSmallBuffer = 4 * 1024;
        const int kLargeBuffer = 4 * 1024 * 1024;

        //获取日志文件名
        std::string getLogFileName();

        //Buffer类：用于缓存日志数据
        template <int SIZE>
        class FixBuffer
        {
        public:
            FixBuffer() : cur_(data_) {}

            ~FixBuffer() {}

            // 禁用拷贝构造和拷贝赋值
            FixBuffer(const FixBuffer &) = delete;
            FixBuffer &operator=(const FixBuffer &) = delete;

            // 因为声明了析构函数，移动构造和移动赋值也会被删除

            //计算当前长度
            size_t size() const noexcept { return cur_ - data_; }
            //可用空间
            size_t avail() const noexcept { return end() - cur_; }
            //返回缓冲区数据
            const char *data() const noexcept { return data_; }
            //将数据复制到cur_中，并增加len个长度
            void append(const char *data, size_t len)
            {
                if (avail() > len)
                {
                    memcpy(cur_, data, len);
                    cur_ += len;
                }
            }
            //返回cur_
            char *current() const noexcept { return cur_; }
            //将cur_指针移动len个长度
            void add(size_t len) { cur_ += len; }
            //将data_转换为string
            std::string toString() const
            {
                return std::string(data_, size());
            }
            //data_赋值给cur_
            void reset() { cur_ = data_; }

        private:
            //返回尾指针
            const char *end() const { return data_ + sizeof(data_); }
            char data_[SIZE];   //缓冲区
            char *cur_;         //缓冲区指针
        };

    } // end of namespace detail

    //日志输出流：重载输出运算符，类对象可以使用 <<
    class LogStream
    {
    public:
        using Buffer = detail::FixBuffer<detail::kSmallBuffer>;
        LogStream() = default;
        ~LogStream() = default;
        //禁止拷贝
        LogStream(const LogStream &) = delete;
        LogStream &operator=(const LogStream &) = delete;

        /**重载输出运算符 <<，<< 之后的变量保存在buffer中，而不是输出到控制台中**/
        LogStream &operator<<(bool v)
        {
            _buffer.append(v ? "1" : "0", 1);
            return *this;
        }

        LogStream &operator<<(short);
        LogStream &operator<<(unsigned short);
        LogStream &operator<<(int);
        LogStream &operator<<(unsigned int);
        LogStream &operator<<(long);
        LogStream &operator<<(unsigned long);
        LogStream &operator<<(long long);
        LogStream &operator<<(unsigned long long);
        LogStream &operator<<(const void *);
        LogStream &operator<<(double);
        LogStream &operator<<(float v)
        {
            *this << static_cast<double>(v);
            return *this;
        }
        LogStream &operator<<(char v)
        {
            _buffer.append(&v, 1);
            return *this;
        }
        LogStream &operator<<(const char *str)
        {
            if (str)
            {
                _buffer.append(str, strlen(str));
            }
            else
            {
                _buffer.append("(null)", 6);
            }
            return *this;
        }
        LogStream &operator<<(const unsigned char *str)
        {
            *this << reinterpret_cast<const char *>(str);
            return *this;
        }
        LogStream &operator<<(const std::string &v)
        {
            _buffer.append(v.c_str(), v.size());
            return *this;
        }

        //追加长度和内容
        void append(const char *data, size_t len) { _buffer.append(data, len); }
        //获得_buffer
        const Buffer &buffer() const noexcept { return _buffer; }
        //重置_buffer
        void reset() { _buffer.reset(); }

    private:
        //格式化整型数据
        template <typename T>
        void formatInteger(T);

    private:
        //数据流缓冲区
        Buffer _buffer;
    };

} // end of namespace clog

#endif // CLOG_LOGSTREAM_H
