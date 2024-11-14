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

// 实例的初始化放在函数内部，是单例模式中的懒汉模式（在第一次被使用才会进行初始化）
connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;
	m_Port = Port;
	m_User = User;
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
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

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


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;

	reserve.wait();
	
	lock.lock();

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
			mysql_close(con);
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

/*
 * 下面是对数据库进行操作的类
*/
// 查询并返回所有博客记录
vector<Blog> sql_blog_tool::select_all_blog(){
	connection_pool* connpool = connection_pool::GetInstance();

	vector<Blog> blogs;
	MYSQL* mysql = NULL;
	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 查询博客数据
	if(mysql_query(mysql, "SELECT blogId, title, content, userId, postTime FROM blog order by postTime desc")){
		return {};
	}

	// 获取结果集
	MYSQL_RES* result = mysql_store_result(mysql);

	// 检查结果集是否为空
	if(!result){
		return {};
	}

	// 获取字段数量
	int num_fields = mysql_num_fields(result);

	// 从结果集中逐行获取数据
	while(MYSQL_ROW row = mysql_fetch_row(result)){
		Blog blog;
		blog.set_blog_id(stoi(row[0]));
		blog.set_blog_title(row[1]);
		blog.set_blog_content(row[2]);
		blog.set_user_id(stoi(row[3]));
		blog.set_blog_postTime(row[4]);

		// 将博客数据存入容器
		blogs.push_back(blog);
	}

	// 释放结果集
	mysql_free_result(result);
	return blogs;
}

// 通过博客id查询博客内容 
Blog sql_blog_tool::select_blog_by_id(int blogid)
{
	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;

	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 查询数据
	string query = "SELECT blogId, title, content, userId, postTime FROM blog WHERE blogId = " + to_string(blogid);
	if(mysql_query(mysql, query.c_str())){
		return {};
	}

	// 获取结果集
	MYSQL_RES* result = mysql_store_result(mysql);
	if(!result){
		return {};
	}

	// 检查结果集是否有数据
	MYSQL_ROW row = mysql_fetch_row(result);
	if(!row){
		mysql_free_result(result);
		return {};
	}

	// 创建Blog对象填充数据
	Blog blog;
	blog.set_blog_id(stoi(row[0]));
	blog.set_blog_title(row[1]);
	blog.set_blog_content(row[2]);
	blog.set_user_id(stoi(row[3]));
	blog.set_blog_postTime(row[4]);

	mysql_free_result(result);
    return blog;
}

// 将用户post过来的博客内容存储数据库
void sql_blog_tool::insert_blog(Blog blog)
{
	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;

	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");
	
	// 预处理SQL语句
	const char* query = "insert into blog(title, content, userId, postTime) values(?, ?, ?, ?)";
	MYSQL_STMT* stmt = mysql_stmt_init(mysql);

	if(!stmt){
		cerr << "mysql_stmt_init() failed" << endl;
		return;
	}

	if(mysql_stmt_prepare(stmt, query, strlen(query))){
		cerr << "mysql_stmt_prepare() failed" << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return;
	}

	// 获取Blog对象的数据
	string title = blog.get_blog_title();
	// cout << "title = " << title << endl;
	string content = blog.get_blog_content();
	// cout << "content = " << content << endl;
	int userId = blog.get_user_id();
	string postTime = blog.get_blog_postTime();

	// 绑定参数
	MYSQL_BIND bind[4];
	memset(bind, 0, sizeof(bind));

	// 绑定每个字段的数据
	bind[0].buffer_type = MYSQL_TYPE_STRING;
	bind[0].buffer = (char*)title.c_str();
	bind[0].buffer_length = title.size();

	bind[1].buffer_type = MYSQL_TYPE_STRING;
	bind[1].buffer = (char*)content.c_str();
	bind[1].buffer_length = content.size();

	bind[2].buffer_type = MYSQL_TYPE_LONG;
	bind[2].buffer = (char*)&userId;

	bind[3].buffer_type = MYSQL_TYPE_STRING;
	bind[3].buffer = (char*)postTime.c_str();
	bind[3].buffer_length = postTime.length();

	// 将绑定的数据应用到语句
	if(mysql_stmt_bind_param(stmt, bind)){
		cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return;
	}

	// 执行语句
	if(mysql_stmt_execute(stmt)){
		cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;

	}else{
		cout << "Blog inserted successfully" << endl;
	}

	// 关闭语句
	mysql_stmt_close(stmt);
}

// 通过用户名获取用户id
int sql_blog_tool::get_userid(string username)
{
	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;

	connectionRAII mysqlconn(&mysql, connpool);

	// 构建SQL查询语句
	string query = "select userId from user where username = '" + username + "'";

	// 执行SQL查询
	if(mysql_query(mysql, query.c_str())){
		cerr << "Query failed: " << mysql_error(mysql) << endl;
		return -1;
	}

	// 获取查询结果
	MYSQL_RES* res = mysql_store_result(mysql);
	if(!res){
		cerr << "Failed to retriee result set :" << mysql_error(mysql) << endl;
		return -1;
	}

	// 获取第一行数据(用户名唯一)
	MYSQL_ROW row = mysql_fetch_row(res);
	int userid = -1;
	if(row){
		userid = atoi(row[0]);
	}

	// 释放结果集
	mysql_free_result(res);
	return userid;
}

/*
 *下面的内容为用户类
*/
void User::set_userid(int userid){
	this->m_userId = userid;
}

void User::set_username(string username){
	this->m_username = username;
}

void User::set_password(string password){
	this->m_password = password;
}

/*
 *下面的内容为博客类
*/
void Blog::set_blog_id(int blog_id){
	this->m_blog_id = blog_id;
}

int Blog::get_blog_id()
{
    return this->m_blog_id;
}

void Blog::set_blog_title(string blog_title){
	this->m_blog_title = blog_title;
}

string Blog::get_blog_title()
{
    return this->m_blog_title;
}

void Blog::set_blog_content(string content){
	this->m_bolg_content = content;
}

string Blog::get_blog_content()
{
    return this->m_bolg_content;
}

void Blog::set_user_id(int user_id){
	this->m_user_id = user_id;
}

int Blog::get_user_id()
{
    return this->m_user_id;
}

void Blog::set_blog_postTime(string blog_postTime){
	this->m_bolg_postTime = blog_postTime;
}

string Blog::get_blog_postTime()
{
    return this->m_bolg_postTime;
}
