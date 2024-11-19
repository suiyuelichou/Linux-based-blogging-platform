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

// 使用分页查询，避免一次性查询所有博客导致速度变慢
// page：页数  size：每页的博客数
vector<Blog> sql_blog_tool::get_blogs_by_page(int page, int size)
{
	// 用于存储查询到的博客
	vector<Blog> blogs;

	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;

	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 预处理SQL语句
	const char* query = "SELECT blogId, title, content, userId, postTime FROM blog ORDER BY postTime DESC LIMIT ? OFFSET ?";

	MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 计算偏移量
    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&size;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 获取结果
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    int blogId;
    char title[256];
    char content[1024];
    int userId;
    char postTime[32];

    MYSQL_BIND result_bind[5];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&blogId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = (char*)title;
    result_bind[1].buffer_length = sizeof(title);

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = (char*)content;
    result_bind[2].buffer_length = sizeof(content);

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = (char*)&userId;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = (char*)postTime;
    result_bind[4].buffer_length = sizeof(postTime);

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Blog blog;
        blog.set_blog_id(blogId);
        blog.set_blog_title(string(title));
        blog.set_blog_content(string(content));
        blog.set_user_id(userId);
        blog.set_blog_postTime(string(postTime));
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 获取博客的总条数
int sql_blog_tool::get_total_blog_count()
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句
    const char* query = "SELECT COUNT(*) FROM blog;";

    // 执行查询
    if (mysql_query(mysql, query)) {
        // 查询失败，打印错误信息
        std::cerr << "MySQL query failed: " << mysql_error(mysql) << std::endl;
        return 0;
    }

    // 获取查询结果
    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        // 获取结果失败
        std::cerr << "MySQL store result failed: " << mysql_error(mysql) << std::endl;
        return 0;
    }

    // 从结果集中提取总条数
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = 0;
    if (row && row[0]) {
        count = std::atoi(row[0]); // 将字符串转为整数
    }

    // 释放结果集
    mysql_free_result(result);

    return count;
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

// 通过博客id获取对应的用户id
int sql_blog_tool::get_userid_by_blogid(int blogid)
{
    // 从连接池中获取 MySQL 连接
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集为 UTF8
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句
    const char* query = "SELECT userId FROM blog WHERE blogId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1; // 错误返回 -1
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1; // 错误返回 -1
    }

    // 绑定参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1; // 错误返回 -1
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1; // 错误返回 -1
    }

    // 绑定结果
    MYSQL_BIND bind_result[1];
    memset(bind_result, 0, sizeof(bind_result));

    int userId;
    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&userId;

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1; // 错误返回 -1
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt) == 0) {
        // 成功获取到 userId
        mysql_stmt_close(stmt);
        return userId;
    } else {
        cerr << "mysql_stmt_fetch() failed or no result found" << endl;
        mysql_stmt_close(stmt);
        return -1; // 未找到结果或发生错误
    }
}

// 通过用户id获取用户信息
User sql_blog_tool::get_userdata_by_userid(int userid)
{
    // 从连接池中获取 MySQL 连接
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集为 UTF8
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句
    const char* query = "SELECT userId, username, password, avatar, article_count, register_time, email FROM user WHERE userId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return User(); // 返回空用户对象
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }

    // 绑定查询参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&userid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }

    // 绑定结果
    MYSQL_BIND bind_result[7];
    memset(bind_result, 0, sizeof(bind_result));

    int userId;
    char username[128];
    char password[128];
    char avatar[256];
    int article_count;
    char register_time[64];
    char email[128];

    // 绑定字段
    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&userId;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = (char*)username;
    bind_result[1].buffer_length = sizeof(username);

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = (char*)password;
    bind_result[2].buffer_length = sizeof(password);

    bind_result[3].buffer_type = MYSQL_TYPE_STRING;
    bind_result[3].buffer = (char*)avatar;
    bind_result[3].buffer_length = sizeof(avatar);

    bind_result[4].buffer_type = MYSQL_TYPE_LONG;
    bind_result[4].buffer = (char*)&article_count;

    bind_result[5].buffer_type = MYSQL_TYPE_STRING;
    bind_result[5].buffer = (char*)register_time;
    bind_result[5].buffer_length = sizeof(register_time);

    bind_result[6].buffer_type = MYSQL_TYPE_STRING;
    bind_result[6].buffer = (char*)email;
    bind_result[6].buffer_length = sizeof(email);

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }
    // 获取结果
    if (mysql_stmt_fetch(stmt) == 0) {
        // 填充用户对象
        User user;
        user.set_userid(userId);
        user.set_username(username);
        user.set_password(password);
        user.set_avatar(avatar);
        user.set_article_count(article_count);
        user.set_register_time(register_time);
        user.set_email(email);

        mysql_stmt_close(stmt);
        return user;
    } else {
        cerr << "mysql_stmt_fetch() failed or no result found" << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }
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

int User::get_userid()
{
    return this->m_userId;
}

void User::set_username(string username){
	this->m_username = username;
}

string User::get_usernmae()
{
    return this->m_username;
}

void User::set_password(string password){
	this->m_password = password;
}

string User::get_password()
{
    return this->m_password;
}

void User::set_avatar(string avatar)
{
	this->m_avatar = avatar;
}

string User::get_avatar()
{
    return this->m_avatar;
}

void User::set_article_count(int article_count)
{
	this->m_article_count = article_count;
}

int User::get_article_count()
{
    return this->m_article_count;
}

void User::set_register_time(string register_time)
{
	this->m_register_time = register_time;
}

string User::get_register_time()
{
    return this->m_register_time;
}

void User::set_email(string email)
{
	this->m_email = email;
}

string User::get_eamil()
{
    return this->m_email;
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
