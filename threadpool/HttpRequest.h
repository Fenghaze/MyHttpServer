/**
 * @author: fenghaze
 * @date: 2021/07/15 09:47
 * @desc: 
 */

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H
#include <string>
#include <map>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include "HttpServer.h"
#include "../lock/locker.h"
#include "../log/AsyncLogger.h"
#include "../log/Logger.h"
extern std::map<std::string, std::string> users; //保存post请求体
extern locker m_userslock;                       //保护users临界资源

class HttpRequest
{
public:
    /*HTTP请求方法，仅支持GET、POST*/
    enum METHOD
    {
        GET = 0,
        POST
    };

    /*解析客户请求时，状态机所处的状态*/
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, //请求行
        CHECK_STATE_HEADER,          //请求头
        CHECK_STATE_CONTENT          //请求体
    };

    /*服务器处理HTTP请求的返回结果*/
    enum HTTP_CODE
    {
        NO_REQUEST = 0,    //没有请求
        GET_REQUEST,       //获得请求
        BAD_REQUEST,       //错误请求
        NO_RESOURCE,       //没有资源
        FORBIDDEN_REQUEST, //禁止请求
        FILE_REQUEST,      //html资源文件请求
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
    HttpRequest() {}
    ~HttpRequest() {}
    //初始化request
    void init()
    {
        m_state = CHECK_STATE_REQUESTLINE;
        m_method = GET;
        m_version = 0;
        m_read_idx = 0;
        m_checked_idx = 0;
        m_start_line = 0;
        memset(m_read_buf, '\0', READ_BUFFER_SIZE);
        memset(m_real_file, '\0', FILENAME_LEN);
    }

public:
    //读取客户数据（request data），仅支持LT模式
    bool read();

    //解析HTTP请求
    HTTP_CODE process_request();

public:
    //设置cfd
    void set_cfd(int cfd) { m_cfd = cfd; }

    //获得html资源文件的内存地址
    char *get_file_address() { return m_file_address; }

    struct stat get_file_stat() { return m_file_stat; }

    bool get_linger() { return m_linger; }

private:
    //检查是否是完整的行：'\r\n'
    LINE_STATUS check_line();
    //解析请求行
    HTTP_CODE parse_requestline(char *text);
    //解析请求头
    HTTP_CODE parse_headers(char *text);
    //解析请求体，POST请求才会有请求体，判断请求体是否被完整地读入
    HTTP_CODE parse_content(char *text);

    /*当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性。
如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址m_file_address处，并告诉调用者获取文件成功*/
    HTTP_CODE do_request();
    //获取一行数据
    char *get_line() { return m_read_buf + m_start_line; }

private:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;                  //读缓冲区的大小
    const char *doc_root = "/home/zhl/桌面/MyHttpServer/html"; //网站根路径
    int m_cfd;                                                 //连接cfd
    CHECK_STATE m_state;                                       //初始状态
    char m_read_buf[READ_BUFFER_SIZE];                         //存放http request的读缓冲区
    int m_read_idx;                                            //标识读缓冲中已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;                                         //当前正在分析的字符在读缓冲区中的位置
    int m_start_line;                                          //当前正在解析的行的起始位置

    METHOD m_method; //请求方法
    char *m_url;     //请求url
    char *m_version; //请求HTTP版本号
    int cgi;         //是否启用的POST

    int m_content_length; //请求体长度
    bool m_linger;        //请求头Keep-Alive
    char *m_host;         //请求头Host
    char *m_string;       //存储请求头数据

    char m_real_file[FILENAME_LEN]; //HTML资源文件名
    struct stat m_file_stat;        //文件属性

    char *m_file_address; //html资源文件的内存地址
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

HttpRequest::LINE_STATUS HttpRequest::check_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        ////printf("m_read_idx=%d, m_checked_idx=%d\n", m_read_idx, m_checked_idx);
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            // '\r'是最后一个字符：不是完整的行
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            // '\r\n'出现：是完整的行
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            //其他情况：行出错
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            //'\r\n'出现：是完整的行
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            //其他情况：行出错
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
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
        //std::cout << "parse line:" << line << std::endl;
        LOG_TRACE << "parse line:" << line;
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

HttpRequest::HTTP_CODE HttpRequest::parse_requestline(char *text)
{
    //std::cout << "parse_requestline()" << std::endl;
    //LOG_TRACE << "parse_requestline()";
    //text = "GET / HTTP/1.1";
    m_url = strpbrk(text, " \t"); // m_url = " / HTTP/1.1";
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // m_url = "/ HTTP/1.1";
    char *method = text;
    //判断字符串是否相等(忽略大小写)
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    //检索m_url 中第一个不在字符串" \t"中出现的字符下标
    m_url += strspn(m_url, " \t");     //m_url = "/ HTTP/1.1";
    m_version = strpbrk(m_url, " \t"); // m_version = " HTTP/1.1";
    *m_version++ = '\0';               // m_version = "HTTP/1.1";
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    //比较参数s1 和s2 字符串前n个字符
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        //在m_url所指向的字符串中搜索第一次出现字符'/'的位置
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，返回判断界面
    if (strlen(m_url) == 1)
    {
        //把"index.html"所指向的字符串追加到m_url所指向的字符串的结尾
        //printf("return judge.html\n");
        strcat(m_url, "judge.html");
    }
    //推动状态机：解析请求头
    m_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::parse_headers(char *text)
{
    //std::cout << "parse_headers()" << std::endl;
    //LOG_TRACE << "parse_headers()";
    //遇到空行，表示头部字段解析完毕
    if (text[0] == '\0')
    {
        //如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体
        if (m_content_length != 0)
        {
            //推动状态机：解析请求体
            m_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        //否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    }
    //处理Connection头部字段
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    //处理Content-Length头部字段
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    //处理Host头部字段
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    //其他字段不处理
    else
    {
        //printf("unknow header, %s\n", text);
        LOG_INFO << "oop!unknow header: " << text;
    }
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::parse_content(char *text)
{
    //std::cout << "parse_content()" << std::endl;
    //LOG_TRACE << "parse_content()";
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求，表单中输入用户名和密码，这行数据保存在m_string中
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpRequest::HTTP_CODE HttpRequest::do_request()
{
    //std::cout << "do_request()" << std::endl;
    //LOG_TRACE << "do_queset()";
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    /*
    post    -> html
    "/0"   返回注册页面    register.html
    "/1"   返回登录页面    log.html
    "/2"   登录，返回欢迎页面 welcome.html
    "/3"   注册，返回登录页面    log.html
    "/4"   返回图片资源    picture.html
    "/5"   返回视频资源    video.html
    */
    //在m_url中搜索最后一次出现字符 c（一个无符号字符）的位置，返回最后一个c之后的子串
    const char *p = strrchr(m_url, '/');
    //处理POST请求（注册、登录请求）
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        char flag = m_url[1]; //0表示注册、1表示登录
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //m_string保存了post发送的用户名和密码，使用'&'进行连接
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        //printf("user=%s\tpassword=%s\n", name, password);
        LOG_WARN << "user = " << name << "\tpassword = " << password;
        //登录验证
        if (*(p + 1) == '2')
        {
            //若浏览器端输入的用户名和密码在表中可以查找到，返回欢迎页面
            if (users.find(name) != users.end() && users[name] == password)
            {
                //printf("return welcome.html\n");
                strcpy(m_url, "/welcome.html");
            }
            else
                strcpy(m_url, "/logError.html");
        }
        //注册：线程安全，当多个用户同时注册，且用户名相同时，需要互斥
        else if (*(p + 1) == '3')
        {
            //如果不存在这条新数据，则插入
            if (users.find(name) == users.end())
            {
                m_userslock.lock();
                users[name] = password;
                m_userslock.unlock();
                //printf("return log.html\n");
                strcpy(m_url, "/log.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
    }

    //get请求
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //printf("return register.html\n");
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //printf("return log.html\n");
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '4')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //printf("return picture.html\n");
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //printf("return video.html\n");
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    //mmap将硬盘上的html资源映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    //所在的内存地址为m_file_address，之后response
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

#endif // HTTPREQUEST_H