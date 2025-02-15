#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <cstring>
#include <map>
#include <string>
#include <sstream>
#include <unordered_map>
#include <iomanip>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"
#include "../cookie/cookie.h"
#include "../CGImysql/sql_tool.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;        // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 65535;   // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;  // 写缓冲区大小

    // HTTP 请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };

    // HTTP 请求的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,    // 解析请求行
        CHECK_STATE_HEADER,             // 解析请求头部
        CHECK_STATE_CONTENT             // 解析请求体（如post请求的数据）
    };

    // HTTP请求处理的状态码
    enum HTTP_CODE
    {
        NO_REQUEST,          // 尚未接收到请求
        GET_REQUEST,         // 成功解析到一个GET请求
        BAD_REQUEST,         // 请求格式错误
        NO_RESOURCE,         // 请求的资源不存在
        FORBIDDEN_REQUEST,   // 请求被拒绝（例如，权限问题）
        FILE_REQUEST,        // 请求成功，文件准备好返回
        INTERNAL_ERROR,      // 服务器内部错误
        CLOSED_CONNECTION,   // 连接已关闭
        BLOG_DATA,          // 博客数据请求成功，准备返回
        BLOG_DETAIL,       // 返回博客详情
        BLOG_USER_HOME,
        LOGIN_REQUEST,       // 登录请求
        REDIRECT,            // 重定向到登录界面
        REDIRECT_HOME,       // 重定向到游客主页面
        REDIRECT_USER_HOME,       // 重定向到用户主页面
        OK         // 返回一个请求成功
    };

    // 行状态
    enum LINE_STATUS
    {
        LINE_OK = 0,    // 当前行解析成功
        LINE_BAD,       // 当前行解析失败
        LINE_OPEN       // 当前行尚未结束，可能需要继续读取更多数据以完成解析。这通常出现在分块传输编码等情况下
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);   // 初始化HTTP连接
    void close_conn(bool real_close = true);    // 关闭连接
    void process();     // 处理HTTP请求
    bool read_once();   // 读取数据
    bool write();       // 写数据
    sockaddr_in *get_address() // 获取客户端地址
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);   // 初始化MYSQL结果

    // 计时器标志和改进标志
    int timer_flag;
    int improv;


private:
    void init();                 // 初始化内部状态
    HTTP_CODE process_read();    // 处理读取的数据
    bool process_write(HTTP_CODE ret);           // 处理写入的数据
    HTTP_CODE parse_request_line(char *text);    // 解析请求行
    HTTP_CODE parse_headers(char *text);         // 解析请求头
    HTTP_CODE parse_content(char *text);         // 解析请求内容
    HTTP_CODE do_request();    // 执行请求
    char *get_line() { return m_read_buf + m_start_line; };    // 获取当前行
    LINE_STATUS parse_line();    // 解析行
    void unmap();    // 取消映射文件
    bool add_response(const char *format, ...);             // 添加响应内容
    bool add_content(const char *content);                  // 添加内容
    bool add_status_line(int status, const char *title);    // 添加状态行
    bool add_headers(int content_length);                   // 添加头部信息
    bool add_content_type();                                // 添加内容类型
    bool add_content_length(int content_length);            // 添加内容长度
    bool add_linger();         // 添加 Keep-Alive
    bool add_blank_line();    // 添加空行
    unordered_map<string, string> parse_post_data(const string& body);  // 解析post请求体的内容
    string url_decode(const string &str);   // url解码函数
    string handle_file_upload(const string& boundary, const string& body, const string& upload_dir);  // 用于解析 multipart/form-data 并保存文件
    string sanitize_filename(const std::string &filename);  // 移除文件名中的潜在危险字符
    string generate_unique_filename(const std::string &filename);   // 生成一个唯一的文件名，避免文件名冲突

public:
    static int m_epollfd;   //epoll文件描述符
    static int m_user_count;    // 用户计数
    MYSQL *mysql;   // MYSQL连接指针
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;                 // 套接字文件描述符
    sockaddr_in m_address;        // 客户端地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    long m_read_idx;              // 读缓冲区索引
    long m_checked_idx;           // 已检查的索引
    int m_start_line;             // 当前行起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
    int m_write_idx;              // 写缓冲区索引
    CHECK_STATE m_check_state;    // 当前检查状态
    METHOD m_method;              // HTTP请求方法方法
    char m_real_file[FILENAME_LEN]; // 真实文件路径
    char *m_url;                  // URL
    char *m_version;              // HTTP 版本
    char *m_host;                 // 主机
    char *m_content_type;
    long m_content_length;        // 内容长度
    int m_content_start;        // 请求体的起始位置
    string m_content;           // 请求体的具体内容
    bool m_linger;                // 是否保持连接
    char *m_file_address;         // 文件地址
    struct stat m_file_stat;      // 文件状态
    struct iovec m_iv[2];         // IO 向量
    int m_iv_count;               // IO 向量计数
    int cgi;                     // 是否启用 POST 请求
    char *m_string;              // 存储post请求体的数据
    string m_post_content;        // 新的存储post请求体的数据
    string m_boundary;
    int bytes_to_send;           // 需要发送的字节数
    int bytes_have_send;         // 已发送的字节数
    char *doc_root;              // 文档根目录

    map<string, string> m_users; // 用户信息
    int m_TRIGMode;              // 触发模式
    int m_close_log;            // 关闭日志标志

    char sql_user[100];         // SQL 用户名
    char sql_passwd[100];       // SQL 密码
    char sql_name[100];         // SQL 数据库名称

    string current_username;    // 每个连接独立的用户名
    bool islogin;        // 用于标志是否已经登录
    string jsonData;    // 用于存储json数据
    Cookie cookie;      // Cookie对象
};

#endif
