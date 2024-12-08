#ifndef _SQL_TOOL_
#define _SQL_TOOL_

#include<string>
#include<vector>
#include<iostream>

using namespace std;

// 用户类，每个User对象就对应一个user表中的一条记录
class User{
public:
	void set_userid(int userid);			// 设置用户id
	int get_userid();
	void set_username(string username);		// 设置用户名
	string get_username();
	void set_password(string password);		// 设置用户密码
	string get_password();
	void set_avatar(string avatar);			// 设置用户头像路径
	string get_avatar();
	void set_article_count(int article_count);// 设置发布的文章数
	int get_article_count();
	void set_register_time(string register_time);// 设置注册时间
	string get_register_time();
	void set_email(string email);// 设置用户邮箱
	string get_eamil();
	void set_description(string description);	// 设置用户简介
	string get_description();		

private:
	int m_userId;		// 用户id
	string m_username;	// 用户名
	string m_password;	// 用户密码
	string m_avatar;	// 用户头像路径
	int m_article_count;// 发布的文章数
	string m_register_time;	// 注册时间
	string m_email;		// 用户邮箱
	string description; // 用户简介
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

// 评论类，用于存储每条博客里的用户评论
class Comments{
public:
	void set_comment_id(int comment_id);
	int get_comment_id();
	void set_blog_id(int blog_id);
	int get_blog_id();
	void set_username(string username);
	string get_username();
	void set_content(string content);
	string get_content();
	void set_parent_id(int parent_id);
	int get_parent_id();
	void set_comment_time(string comment_time);
	string get_comment_time();

private:
	int m_comment_id;
	int m_blog_id;
	string m_username;
	string m_content;
	int m_parent_id;
	string m_comment_time;
};

// 对blog和user表进行数据库操作
class sql_blog_tool{
public:
	vector<Blog> select_all_blog();						// 查询所有博客
	vector<Blog> get_blogs_by_page(int page, int size);	// 分页查询
	int get_total_blog_count();							// 获取博客的总条数
	Blog select_blog_by_id(int blogid);					// 通过博客id查询博客内容
	int get_userid_by_blogid(int blogid);				// 通过博客id获取对应的用户id
	void modify_blog_by_blogid(Blog blog);				// 通过博客id修改博客的标题和内容
	void delete_blog_by_blogid(int blogid);				// 通过博客id删除指定博客
	vector<Comments> get_comments_by_blogid(int blogid);		// 通过博客id获取对应的评论内容
	void add_comment_by_blogid(Comments comment);		// 通过博客id插入评论
	User get_userdata_by_userid(int userid); 			// 通过用户id获取用户信息
	vector<Blog> get_blogs_by_userid(int userid);	// 通过用户id获取该用户的所有博客
	void insert_blog(Blog blog);						// 将用户post过来的博客内容存储数据库
	int get_userid(string username);					// 通过用户名获取用户id
	void modify_password_by_username(string username, string password);	// 通过用户名修改用户密码


public:
	int m_close_log;	// 日志开关
};

#endif