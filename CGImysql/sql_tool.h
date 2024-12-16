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

// 消息类，用于存储系统信息、点赞信息、评论信息等
class Messages{
public:
	void set_message_id(int message_id);
	int get_message_id();
	void set_sender_id(int sender_id);
	int get_sender_id();
	void set_recipient_id(int recipient_id);
	int get_recipient_id();
	void set_blog_id(int blog_id);
	int get_blog_id();
	void set_type(string type);
	string get_type();
	void set_content(string content);
	string get_content();
	void set_post_time(string post_time);
	string get_post_time();
	void set_is_read(bool is_read);
	bool get_is_read();

private:
	int m_message_id;
	int m_sender_id;
	int m_recipient_id;
	int m_blog_id;
	string m_type;		// 消息类型
	string m_content;
	string m_post_time;
	bool m_is_read;		// 1:read	0：unread 默认为0
};

// 博客点赞类，用于存储用户对博客的点赞信息
class Blog_like{
public:
	void set_like_id(int like_id);
	int get_like_id();
	void set_user_id(int user_id);
	int get_user_id();
	void set_blog_id(int blog_id);
	int get_blog_id();
	void set_like_time(string like_time);
	string get_like_time();

private:
	int m_like_id;
	int m_user_id;
	int m_blog_id;
	string m_like_time;
};

// 对blog和user表进行数据库操作
class sql_blog_tool{
public:
	vector<Blog> select_all_blog();						// 查询所有博客
	vector<Blog> get_blogs_by_page(int page, int size);	// 分页查询博客
	int get_total_blog_count();							// 获取博客的总条数
	Blog select_blog_by_id(int blogid);					// 通过博客id查询博客内容
	int get_userid_by_blogid(int blogid);				// 通过博客id获取对应的用户id
	void modify_blog_by_blogid(Blog blog);				// 通过博客id修改博客的标题和内容
	void delete_blog_by_blogid(int blogid);				// 通过博客id删除指定博客
	vector<Comments> get_comments_by_blogid(int blogid);		// 通过博客id获取对应的评论内容
	void add_comment_by_blogid(Comments comment);		// 通过博客id插入评论
	User get_userdata_by_userid(int userid); 			// 通过用户id获取用户信息
	vector<Blog> get_blogs_by_userid(int userid);		// 通过用户id获取该用户的所有博客
	vector<Messages> get_messages_by_userid(int userid);// 通过用户id获取该用户收到的所有信息
	void insert_blog(Blog blog);						// 将用户post过来的博客内容存储数据库
	int get_userid(string username);					// 通过用户名获取用户id
	void modify_password_by_username(string username, string password);	// 通过用户名修改用户密码

	// 个人中心-消息中心相关
	bool check_message_belongs_to_user(int userid, int messageid);	// 检查消息是否属于该用户
	bool mark_message_as_read(int messageid);			// 标记消息为已读
	bool mark_all_message_as_read(int userid);			// 标记所有消息为已读
	bool delete_message(int messageid);					// 删除指定消息
	bool insert_new_message(Messages message);			// 插入新消息

	// 点赞相关
	bool insert_new_blog_like(Blog_like blog_like);		// 插入新的博客点赞
	bool remove_blog_like(int userid, int blogid);		// 删除博客点赞
	int get_blog_likes_count(int blogid);				// 获取当前博客的点赞总数
	bool is_user_liked_blog(int userid, int blog_id);	// 检测用户是否已经对该博客点赞


public:
	int m_close_log;	// 日志开关
};

#endif