#ifndef CLOG_LOCALTIME_H
#define CLOG_LOCALTIME_H

#include <string>

namespace clog
{

    namespace detail
    {
        //1s = 1000ms = 1000 * 1000us
        const int kMicroSecondsPerSecond = 1000 * 1000;

    } // end of namespace detail

    // Localtime 的对象大小和long long 的对象大小相同
    // 因此以值传递Localtime对象和以引用传递Localtime对象的效率大致相同
    class Localtime
    {
    public:
        Localtime(long long);
        //当前时间        
        static Localtime now();
        //转换为字符串
        std::string toFormattedString(bool showMicroSeconds = true) const;

        long long microSecondsSinceEpoch() const noexcept
        {
            return _microSecondsSinceEpoch;
        }

    private:
        long long _microSecondsSinceEpoch;  //微秒数
    };

    inline bool operator<(Localtime lhs, Localtime rhs)
    {
        return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }

    inline bool operator==(Localtime lhs, Localtime rhs)
    {
        return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }

    inline bool operator>(Localtime lhs, Localtime rhs)
    {
        return lhs.microSecondsSinceEpoch() > rhs.microSecondsSinceEpoch();
    }

    inline bool operator<=(Localtime lhs, Localtime rhs)
    {
        return !(lhs > rhs);
    }

    inline bool operator>=(Localtime lhs, Localtime rhs)
    {
        return !(lhs < rhs);
    }

    // 在原有的时间上加上秒数得到新的时间
    inline Localtime addTime(const Localtime localtime, double seconds)
    {
        long long delta = static_cast<long long>(seconds * detail::kMicroSecondsPerSecond);
        return Localtime(localtime.microSecondsSinceEpoch() + delta);
    }

    // 求两个时间到差值，返回秒数
    inline double timeDifference(Localtime time1, Localtime time2)
    {
        long long diff;

        if (time1 > time2)
            diff = time1.microSecondsSinceEpoch() - time2.microSecondsSinceEpoch();
        else
            diff = time2.microSecondsSinceEpoch() - time1.microSecondsSinceEpoch();

        return static_cast<double>(diff) / detail::kMicroSecondsPerSecond;
    }

} // end of namespace clog

#endif // CLOG_LOCALTIME_H
