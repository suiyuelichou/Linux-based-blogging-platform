#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);     // 将请求request添加到工作队列，并标记任务的状态state(读/写)
    bool append_p(T *request);              // 将请求request添加到工作队列

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg); //使用void*类型，可以传递任意类型的指针
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列，用来存储所有待处理的请求
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //信号量，用于通知是否有任务需要处理
    connection_pool *m_connPool;  //数据库连接池
    int m_actor_model;          //模型切换
};

template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    
    m_threads = new pthread_t[m_thread_number]; //动态分配线程数组
    if (!m_threads)
        throw std::exception();
    
    // 创建线程
    for (int i = 0; i < thread_number; ++i)
    {   //创建了多少个线程就有多少个this指针传递给worker，但是传递的都是同一个（指向同一个线程池实例对象）。
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) //将this指针传递给worker，使其能够访问该线程池中的成员
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))   //分离线程后，当线程任务执行完成，会自动释放资源
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

// 将请求request添加到工作队列，并标记任务的状态state(读/写)
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;   //设置任务状态
    m_workqueue.push_back(request);     //将任务添加到工作队列
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 将请求request添加到工作队列
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 工作线程(子线程)运行的函数，它不断从工作队列(任务队列)中取出任务并执行
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;   //将传入的this指针转换为threadpool，以便访问类实例的函数
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();   // 取出队列中的第一个任务 这个request实际就是一个http_conn对象！！！！！
        m_workqueue.pop_front();            // 移除已经取出的任务
        m_queuelocker.unlock();             // 解锁任务队列
        if (!request)
            continue;
        if (1 == m_actor_model)     //1 Reactor
        {
            if (0 == request->m_state)  //0表示读操作
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else    //1表示写操作
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else                        //0 Proactor
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
#endif
