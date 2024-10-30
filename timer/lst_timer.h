#ifndef LST_TIMER
#define LST_TIMER

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

#include <time.h>
#include "../log/log.h"

class util_timer;

// 存储客户端数据
struct client_data
{
    sockaddr_in address; // 存储客户端地址信息
    int sockfd; // 存储客户端的socket文件描述符
    util_timer *timer; // 指向关联的定时器对象
};

// 实现一个定时器节点
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;  // 存储定时器的过期时间
    
    void (* cb_func)(client_data *);    //函数指针 定时器超时后，会调用cb_func指向的函数
    client_data *user_data; // 指向与定时器关联的客户端数据
    util_timer *prev;
    util_timer *next;
};

// 管理定时器的有序链表
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();    // 检查并处理过期的定时器

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;   // 链表的头指针
    util_timer *tail;   // 链表的尾指针
};

// 提供工具函数，处理与网络编程相关的各种功能
class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;   //管道文件描述符，用于信号处理
    sort_timer_lst m_timer_lst; // 定时器链表实例
    static int u_epollfd;   // epoll文件描述符
    int m_TIMESLOT; // 定时器时间片大小
};

// 定义回调函数，处理定时器超时时间
void cb_func(client_data *user_data);

#endif
