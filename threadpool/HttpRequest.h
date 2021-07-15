/**
 * @author: fenghaze
 * @date: 2021/07/15 09:47
 * @desc: 
 */

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H
#include <string>
#include <map>

class HttpRequest
{
public:
    /*HTTP请求方法，仅支持GET、POST*/
    enum METHOD
    {
        GET = 0,
        POST
    };

    /*HTTP版本号*/
    enum VERSION
    {
        HTTP10 = 0,
        HTTP11,
        UNKNOWN
    };

    /*解析客户请求时，状态机所处的状态*/
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, //请求行
        CHECK_STATE_HEADER,          //请求头
        CHECK_STATE_CONTENT          //请求内容
    };

    /*服务器处理HTTP请求的返回结果*/
    enum HTTP_CODE
    {
        NO_REQUEST = 0,    //没有请求
        GET_REQUEST,       //获得请求
        BAD_REQUEST,       //错误请求
        NO_RESOURCE,       //没有资源
        FORBIDDEN_REQUEST, //禁止请求
        FILE_REQUEST,      //文件请求
        INTERNAL_ERROR,    //服务器错误
        CLOSED_CONNECTION  //关闭连接
    };

    /*行的读取状态*/
    enum LINE_STATUS
    {
        LINE_OK = 0, //读取到一个完整的行
        LINE_BAD,    //行出错
        LINE_OPEN    //不是一个完整的行
    };

public:
    HttpRequest()
    {
        m_state = CHECK_STATE_REQUESTLINE;
        m_method = GET;
        m_version = HTTP11;
        m_read_idx = 0;
        m_checked_idx = 0;
        m_start_line = 0;
        memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    }
    ~HttpRequest() {}

public:
    //读取客户数据（request data），仅支持LT模式
    bool read();
    //检查是否是完整的行
    LINE_STATUS check_line();
    //解析HTTP请求
    HTTP_CODE process_request();

    //解析请求头
    HTTP_CODE parse_headers(char *data);
    //解析请求行
    HTTP_CODE parse_requestline(char *data);
    //解析post请求主体
    HTTP_CODE parse_content(char *data);

    /*当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性。
如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，并告诉调用者获取文件成功*/
    HTTP_CODE do_request();
    //获取一行数据
    char *get_line() { return m_read_buf + m_start_line; }

public:
    //设置cfd
    void set_cfd(int cfd) { m_cfd = cfd; }

private:
    int m_cfd;                                //连接cfd
    CHECK_STATE m_state;                      //初始状态
    METHOD m_method;                          //请求方法
    VERSION m_version;                        //HTTP版本号
    static const int READ_BUFFER_SIZE = 2048; //读缓冲区的大小
    char m_read_buf[READ_BUFFER_SIZE];        //存放http request的读缓冲区
    int m_read_idx;                           //标识读缓冲中已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;                        //当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                         //当前正在解析的行的起始位置
};

bool HttpRequest::read()
{
    //判断是否超出读缓冲区
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    //接收读取到的字符，读缓冲区逐渐缩小
    bytes_read = recv(m_cfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;

    if (bytes_read <= 0)
    {
        return false;
    }
    return true;
}

HttpRequest::LINE_STATUS HttpRequest::check_line(char *data)
{
}

HttpRequest::HTTP_CODE HttpRequest::process_request()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *line = 0;
    //获取到完整行就开始解析request
    while ((m_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = check_line()) == LINE_OK))
    {
        line = get_line();
        m_start_line = m_checked_idx;
        switch (m_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_requestline(line);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(line);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(line);
            if (ret == GET_REQUEST)
            {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::parse_headers(char *data)
{
}

HttpRequest::HTTP_CODE HttpRequest::parse_requestline(char *data)
{
}

HttpRequest::HTTP_CODE HttpRequest::parse_content(char *data)
{
}

#endif // HTTPREQUEST_H