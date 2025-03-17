#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"

const int MAX_FD = 2000;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();     //创建并初始化线程池
    void sql_pool();        //初始化数据库连接池
    void log_write();       //日志模块的初始化
    void trig_mode();       //设置触发模式（水平触发（LT/边缘触发（ET）
    void eventListen();     //进行套接字的监听
    void eventLoop();       //服务器的主循环，用于处理事件
    void timer(int connfd, struct sockaddr_in client_address);      //为新连接设置定时器。管理客户端连接的超时处理
    void adjust_timer(util_timer *timer);                           //调整定时器，用于重置客户端的超时时间
    void deal_timer(util_timer *timer, int sockfd);                 //处理超时的连接并关闭相应的客户端
    bool dealclientdata();                                          //处理新客户端的连接请求
    bool dealwithsignal(bool& timeout, bool& stop_server);          //处理信号事件（如定时器、关闭服务器等）
    void dealwithread(int sockfd);      //处理可读事件
    void dealwithwrite(int sockfd);     //处理可写事件

public:
    //基础
    int m_port;         //服务器监听的端口号
    char *m_root;       //网站根目录
    int m_log_write;    //日志写入方式（同步/异步）
    int m_close_log;    //是否关闭日志
    int m_actormodel;   //处理模式（Reactor/Proactor)

    //HTTP连接相关
    http_conn *users;   //保存每个客户端的连接信息

    //数据库相关
    connection_pool *m_connPool;    //数据库连接池
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;         //数据库连接池中的连接数

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    int m_pipefd[2];    //用于信号处理的管道 数据从 m_pipefd[1] 写入，从 m_pipefd[0] 读取
    int m_epollfd;      //epoll实例的文件描述符
    epoll_event events[MAX_EVENT_NUMBER];   //存储事件的数组

    int m_listenfd;         // 用于监听客户端连接
    int m_OPT_LINGER;       //设置套接字的SO_LINGER选项
    int m_TRIGMode;         //设置触发模式
    int m_LISTENTrigmode;   //监听套接字的触发模式
    int m_CONNTrigmode;     //设置连接的触发模式

    //定时器相关
    client_data *users_timer;
    Utils utils;
};
#endif