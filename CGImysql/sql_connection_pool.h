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
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

// 数据库连接池
class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接
	

	//单例模式
	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

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
	list<MYSQL *> connList; //连接池
	sem reserve;	// 设置一个数据库连接池信号量

public:
	string m_url;			 //主机地址
	string m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
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

// 用户类，每个User对象就对应一个user表中的一条记录
class User{
public:
	void set_userid(int userid);			// 设置用户id
	void get_userid();
	void set_username(string username);		// 设置用户名
	void get_usernmae();
	void set_password(string password);		// 设置用户密码
	void get_password();

private:
	int m_userId;		// 用户id
	string m_username;	// 用户名
	string m_password;	// 用户密码
};

// 博客类，每个Blog对象就对应一个blog表中的一条记录
class Blog{
public:
	void set_blog_id(int blog_id);				// 设置博客id
	int get_blog_id();
	void set_blog_title(string blog_title);		// 设置博客标题
	string get_blog_title();
	void set_blog_content(string content);		// 设置博客内容
	string get_blog_content();
	void set_user_id(int user_id);				// 设置用户id
	int get_user_id();
	void set_blog_postTime(string blog_postTime);	// 设置博客发布时间
	string get_blog_postTime();

private:
	int m_blog_id;			// 博客id
	string m_blog_title;	// 博客标题	
	string m_bolg_content;	// 博客内容
	int m_user_id;			// 用户id
	string m_bolg_postTime;			// 博客发布时间
};

// 对blog表进行数据库操作
class sql_blog_tool{
public:
	vector<Blog> select_all_blog();	// 查询所有博客
	Blog select_blog_by_id(int blogid);		// 通过博客id查询博客内容 

public:
	int m_close_log;	// 日志开关
};

#endif
