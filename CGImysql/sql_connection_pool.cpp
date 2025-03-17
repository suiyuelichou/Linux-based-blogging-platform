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
	m_Port = Port;
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
		// 建立数据库连接
		con = mysql_real_connect(con, url.c_str(), UserName.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		connList.push_back(con);
		++m_FreeConn;
	}

	reserve = sem(m_FreeConn);	// 将当前可使用的连接数量作为信号量的初始值

	m_MaxConn = m_FreeConn;
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

//销毁数据库连接池
void connection_pool::DestroyPool()
{

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
