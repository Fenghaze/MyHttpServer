/**
 * @author: fenghaze
 * @date: 2021/07/15 09:47
 * @desc: 
 */

#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H
#include <sys/mman.h>
#include <sys/uio.h>
#include <map>
#include <string>
#include <stdarg.h>
#include "../utils/utils.h"
#include "HttpRequest.h"
#include "HttpServer.h"

std::map<const int, const char *> RES_CODE = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {500, "Internal Error"}};
std::map<const int, const char *> RES_SOURCE =
    {
        {400, "Your request has bad syntax or is inherently impossible to staisfy.\n"},
        {403, "You do not have permission to get file form this server.\n"},
        {404, "The requested file was not found on this server.\n"},
        {500, "There was an unusual problem serving the request file.\n"}};
class HttpResponse
{
public:
    HttpResponse() { init(); }
    ~HttpResponse() {}

    //初始化request和response对象的属性
    void init()
    {
        request.init();
        m_write_idx = 0;
        bytes_to_send = 0;
        bytes_have_send = 0;
        memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    }
    HttpRequest get_request() { return request; }

public:
    //发送客户数据（response data）
    bool write();

    //处理HTTP响应
    bool process_write(HttpRequest::HTTP_CODE ret);

    //设置cfd
    void set_cfd(int cfd) { m_cfd = cfd; }

private:
    //解除html文件的内存地址映射
    void unmap();

    //设置response
    bool set_response(const char *format, ...);

    //设置响应主体
    bool set_content(const char *content);

    //设置状态
    bool set_status_line(int status);

    //设置响应头
    bool set_headers(int content_length);

    //设置content-type
    bool set_content_type();

    //设置content-length
    bool set_content_length(int content_length);

    //设置keepalive
    bool set_linger();

    //设置空行
    bool set_blank_line();

private:
    static const int WRITE_BUFFER_SIZE = 1024;
    char m_write_buf[WRITE_BUFFER_SIZE]; //发送缓冲区
    int m_write_idx;                     //发送的下一个字节

    int bytes_to_send;   //待发送的数据长度
    int bytes_have_send; //已经发送的数据长度
    int m_cfd;           //连接cfd

    struct iovec m_iv[2]; //IO向量数组，iovec.iov_base存放readv/writev缓冲区数据，iovec.iov_len记录数据长度
    int m_iv_count;

    HttpRequest request; //HttpRequest对象
};

void HttpResponse::unmap()
{
    char *m_file_address = request.get_file_address();
    if (m_file_address)
    {
        munmap(m_file_address, request.get_file_stat().st_size);
        m_file_address = 0;
    }
}

bool HttpResponse::write()
{
    int temp = 0;
    //如果没有数据要发送，则监听EPOLLIN事件，初始化request数据
    if (bytes_to_send == 0)
    {
        modfd(HttpServer::m_epollfd, m_cfd, EPOLLIN);
        init();
        return true;
    }

    //发送数据
    while (1)
    {
        temp = writev(m_cfd, m_iv, m_iv_count);
        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(HttpServer::m_epollfd, m_cfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        //更新待发送和已发送字节数
        bytes_have_send += temp;
        bytes_to_send -= temp;
        //已发送字节数<第一个缓冲区的长度
        if (bytes_have_send < m_iv[0].iov_len)
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        //否则使用第二个IO向量缓冲区
        else
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = request.get_file_address() + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        //无数据可发送，则监听EPOLLIN事件，初始化request数据
        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(HttpServer::m_epollfd, m_cfd, EPOLLIN);
            if (request.get_linger())
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

bool HttpResponse::process_write(HttpRequest::HTTP_CODE ret)
{
    switch (ret)
    {
    case HttpRequest::HTTP_CODE::INTERNAL_ERROR:
    {
        set_status_line(500);
        set_headers(strlen(RES_SOURCE[500]));
        if (!set_content(RES_SOURCE[500]))
        {
            return false;
        }
        break;
    }
    case HttpRequest::HTTP_CODE::BAD_REQUEST:
    {
        set_status_line(400);
        set_headers(strlen(RES_SOURCE[400]));
        if (!set_content(RES_SOURCE[400]))
        {
            return false;
        }
        break;
    }
    case HttpRequest::HTTP_CODE::NO_RESOURCE:
    {
        set_status_line(404);
        set_headers(strlen(RES_SOURCE[404]));
        if (!set_content(RES_SOURCE[404]))
        {
            return false;
        }
        break;
    }
    case HttpRequest::HTTP_CODE::FORBIDDEN_REQUEST:
    {
        set_status_line(403);
        set_headers(strlen(RES_SOURCE[403]));
        if (!set_content(RES_SOURCE[403]))
        {
            return false;
        }
        break;
    }
    case HttpRequest::HTTP_CODE::FILE_REQUEST:
    {
        set_status_line(200);
        if (request.get_file_stat().st_size != 0)
        {
            set_headers(request.get_file_stat().st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = request.get_file_address();
            m_iv[1].iov_len = request.get_file_stat().st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + request.get_file_stat().st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            set_headers(strlen(ok_string));
            if (!set_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool HttpResponse::set_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    std::cout << "request:" << m_write_buf << std::endl;
    return true;
}

bool HttpResponse::set_content(const char *content)
{
    return set_response("%s", content);
}

bool HttpResponse::set_status_line(int status)
{
    return set_response("%s %d %s\r\n", "HTTP/1.1", status, RES_CODE[status]);
}

bool HttpResponse::set_headers(int content_length)
{
    set_content_type();
    set_content_length(content_length);
    set_linger();
    set_blank_line();
}

bool HttpResponse::set_content_type()
{
    return set_response("Content-Type:%s\r\n", "text/html");
}

bool HttpResponse::set_content_length(int content_length)
{
    return set_response("Content-Length:%d\r\n", content_length);
}

bool HttpResponse::set_linger()
{
    return set_response("Connection:%s\r\n", (request.get_linger() == true) ? "keep-alive" : "close");
}

bool HttpResponse::set_blank_line()
{
    return set_response("%s", "\r\n");
}
#endif // HTTPRESPONSE_H