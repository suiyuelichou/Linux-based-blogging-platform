#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
	m_CurConn = 0;
	m_FreeConn = 0;
	keepalive_thread = nullptr;
	running = false;
}

// 实例的初始化放在函数内部，是单例模式中的懒汉模式（在第一次被调用才会进行初始化）
connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

// 初始化数据库连接池
void connection_pool::init(string url, string UserName, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;
	m_Port = Port;  // 直接存储整型值
	m_User = UserName;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}

		// 设置自动重连选项
		my_bool reconnect = 1;
		mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);
		
		// 设置连接超时时间
		int timeout = 10;
		mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
		
		// 设置读取超时时间
		int read_timeout = 30;
		mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &read_timeout);
		
		// 设置写入超时时间
		int write_timeout = 30;
		mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout);

		// 建立数据库连接，直接使用整型端口号
		con = mysql_real_connect(con, url.c_str(), UserName.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}

		// 设置UTF8mb4字符集
		mysql_set_character_set(con, "utf8mb4");

		connList.push_back(con);
		++m_FreeConn;
	}

	reserve = sem(m_FreeConn);	// 将当前可使用的连接数量作为信号量的初始值

	m_MaxConn = m_FreeConn;

	// 启动保活线程
	StartKeepAliveThread();
}

// 检查连接是否有效
bool connection_pool::PingConnection(MYSQL *conn)
{
	if (conn == NULL)
		return false;
	
	// mysql_ping: 如果连接断开会尝试重连，返回0表示连接正常
	return (mysql_ping(conn) == 0);
}

//从数据库连接池中取出一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	reserve.wait();
	
	lock.lock();

	if (connList.empty()){
		LOG_ERROR("The database connection pool is empty!");
		// cout << "The database connection pool is empty!" << endl;
		lock.unlock();
		return NULL;
	}

	con = connList.front();
	connList.pop_front();

	--m_FreeConn;
	++m_CurConn;

	lock.unlock();

	// 检查连接是否有效，如果无效则重新建立连接
	if (!PingConnection(con)) {
		LOG_INFO("Connection lost, reconnecting to MySQL server");
		
		// 关闭失效连接
		mysql_close(con);
		
		// 初始化新连接
		con = mysql_init(NULL);
		if (con == NULL) {
			LOG_ERROR("MySQL init error during reconnect");
			lock.lock();
			--m_CurConn;
			++m_FreeConn;
			reserve.post();
			lock.unlock();
			return NULL;
		}
		
		// 设置自动重连选项
		my_bool reconnect = 1;
		mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);
		
		// 重新连接数据库 - 使用整型端口号
		con = mysql_real_connect(con, m_url.c_str(), m_User.c_str(), m_PassWord.c_str(),
								m_DatabaseName.c_str(), m_Port, NULL, 0);
		
		if (con == NULL) {
			LOG_ERROR("MySQL reconnect error");
			lock.lock();
			--m_CurConn;
			++m_FreeConn;
			reserve.post();
			lock.unlock();
			return NULL;
		}
		
		// 设置UTF8mb4字符集
		mysql_set_character_set(con, "utf8mb4");
	}
	
	return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);	// 将当前连接存入连接池，表示已可用
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();		// 通知正在等待连接的线程
	return true;
}

// 定期保活连接的函数
void connection_pool::KeepAliveConnections()
{
	if (m_close_log == 0)
		LOG_INFO("Database connection keepalive thread started");
	
	while (running) {
		// 每30分钟检查一次连接
		std::this_thread::sleep_for(std::chrono::minutes(30));
		
		if (!running)
			break;
			
		lock.lock();
		
		if (m_close_log == 0)
			LOG_INFO("Checking database connections");
		
		// 遍历所有空闲连接
		list<MYSQL *>::iterator it = connList.begin();
		while (it != connList.end() && running) {
			MYSQL *con = *it;
			
			// 如果连接无效，重新建立连接
			if (!PingConnection(con)) {
				if (m_close_log == 0)
					LOG_INFO("Found invalid connection, recreating");
				
				// 关闭失效连接
				mysql_close(con);
				
				// 初始化新连接
				con = mysql_init(NULL);
				if (con == NULL) {
					LOG_ERROR("MySQL init error during keepalive");
					++it;
					continue;
				}
				
				// 设置自动重连选项
				my_bool reconnect = 1;
				mysql_options(con, MYSQL_OPT_RECONNECT, &reconnect);
				
				// 重新连接数据库 - 使用整型端口号
				con = mysql_real_connect(con, m_url.c_str(), m_User.c_str(), m_PassWord.c_str(),
									   m_DatabaseName.c_str(), m_Port, NULL, 0);
				
				if (con == NULL) {
					LOG_ERROR("MySQL reconnect error during keepalive");
					++it;
					continue;
				}
				
				// 设置UTF8mb4字符集
				mysql_set_character_set(con, "utf8mb4");
				
				// 替换链表中的连接
				*it = con;
			} else {
				// 对有效连接执行一个简单查询保持活跃
				if (mysql_query(con, "SELECT 1") != 0) {
					LOG_ERROR("Keepalive query failed");
				}
			}
			++it;
		}
		
		lock.unlock();
	}
	
	if (m_close_log == 0)
		LOG_INFO("Database connection keepalive thread stopped");
}

// 启动保活线程
void connection_pool::StartKeepAliveThread()
{
	if (!running) {
		running = true;
		keepalive_thread = new std::thread(&connection_pool::KeepAliveConnections, this);
	}
}

// 停止保活线程
void connection_pool::StopKeepAliveThread()
{
	if (running) {
		running = false;
		if (keepalive_thread && keepalive_thread->joinable()) {
			keepalive_thread->join();
			delete keepalive_thread;
			keepalive_thread = nullptr;
		}
	}
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{
	// 停止保活线程
	StopKeepAliveThread();

	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);	// 关闭数据库连接
			con = nullptr;	// 避免悬空指针
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}

// 通过构造函数从地址池里面取出一个数据库连接
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL;		// 两者指向同一个数据库连接（两个指针存放的地址相同）
	poolRAII = connPool;
}

// 通过析构函数释放当前数据库连接
connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}
