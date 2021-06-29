#include "LogStream.h"

namespace clog
{

    namespace detail
    {

        const char digits[] = "9876543210123456789";
        const char *zero = digits + 9;
        static_assert(sizeof(digits) == 20, "size of digits is not equal to 20");

        const char digitsHex[] = "0123456789ABCDEF";
        static_assert(sizeof(digitsHex) == 17, "size of digitsHex is not equal to 17");

        // 把整形数快速转换成字符串，作者Matthew Wilson
        template <typename T>
        size_t convert(char buf[], T value)
        {
            T i = value;
            char *p = buf;

            do
            {
                int lsd = static_cast<int>(i % 10);
                *p++ = zero[lsd];
                i /= 10;
            } while (i != 0);

            if (value < 0)
                *p++ = '-';
            *p = '\0';

            std::reverse(buf, p);

            return p - buf;
        }

        size_t convertHex(char buf[], uintptr_t value)
        {
            uintptr_t i = value;
            char *p = buf;

            do
            {
                int lsd = static_cast<int>(i % 16);
                *p++ = digitsHex[lsd];
                i /= 16;
            } while (i != 0);

            *p = '\0';

            std::reverse(buf, p);

            return p - buf;
        }

        std::string getLogFileName()
        {
            std::string filename("logfile");

            char timebuf[32];
            struct tm tm_time;
            time_t now = time(NULL);
            localtime_r(&now, &tm_time);
            strftime(timebuf, sizeof(timebuf), ".%Y%m%d-%H%M%S", &tm_time);
            filename += timebuf;

            filename += ".log";

            return filename;
        }

    } // end of namespace detail

    //格式化整型数据
    template <typename T>
    void LogStream::formatInteger(T v)
    {
        size_t len = detail::convert(_buffer.current(), v);
        _buffer.add(len);
    }

    LogStream &LogStream::operator<<(short v)
    {
        //转换为int类型，并调用 <<(int v)
        *this << static_cast<int>(v);
        return *this;
    }

    LogStream &LogStream::operator<<(unsigned short v)
    {
        *this << static_cast<unsigned int>(v);
        return *this;
    }

    LogStream &LogStream::operator<<(int v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(unsigned int v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(long v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(unsigned long v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(long long v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(unsigned long long v)
    {
        formatInteger(v);
        return *this;
    }

    LogStream &LogStream::operator<<(const void *p)
    {
        uintptr_t v = reinterpret_cast<uintptr_t>(p);
        char *buf = _buffer.current();
        buf[0] = '0';
        buf[1] = 'x';

        size_t len = detail::convertHex(buf + 2, v);
        _buffer.add(len + 2);

        return *this;
    }

    LogStream &LogStream::operator<<(double v)
    {
        size_t len = snprintf(_buffer.current(), sizeof(double), "%.12g", v);
        _buffer.add(len);

        return *this;
    }

} // end of namespace log
