#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <ctime>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

// 数据库连接池
class connection_pool
{
public:
	MYSQL *GetConnection();				 // 获取数据库连接
	bool ReleaseConnection(MYSQL *conn); // 释放连接
	int GetFreeConn();					 // 获取连接
	void DestroyPool();					 // 销毁所有连接
	bool PingConnection(MYSQL *conn);    // 检查连接是否有效
	void KeepAliveConnections();         // 定期保活连接
	void StartKeepAliveThread();         // 启动保活线程
	void StopKeepAliveThread();          // 停止保活线程

	// 单例模式
	static connection_pool *GetInstance();

	// 对连接池进行初始化
	void init(string url, string UserName, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	// 禁止拷贝构造和赋值
	connection_pool(const connection_pool&) = delete;
	connection_pool& operator = (const connection_pool&) = delete;

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	locker lock;
	sem reserve;	// 设置一个数据库连接池信号量
	list<MYSQL *> connList; //连接池
	
	std::thread *keepalive_thread;     // 保活线程
	std::atomic<bool> running;         // 线程运行标志

public:
	string m_url;			 //主机地址
	int m_Port;		         //数据库端口号(改为整型)
	string m_User;		     //登陆数据库用户名
	string m_PassWord;	     //登陆数据库密码
	string m_DatabaseName;   //使用数据库名
	int m_close_log;	     //日志开关
};

// 用于获取数据库连接 
class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
