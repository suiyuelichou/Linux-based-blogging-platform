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
	void set_status(string status);
	string get_status();
	void set_last_login_time(string last_login_time);
	string get_last_login_time();

private:
	int m_userId;		// 用户id
	string m_username;	// 用户名
	string m_password;	// 用户密码
	string m_avatar;	// 用户头像路径
	int m_article_count;// 发布的文章数
	string m_register_time;	// 注册时间
	string m_email;		// 用户邮箱
	string description; // 用户简介
	string m_status;		// 用户状态
	string last_login_time;// 上次登录时间
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
	void set_category_id(int category_id);
	int get_category_id();
	void set_updatedAt(string updatedAt);
	string get_updatedAt();
	void set_views(int views);
	int get_views();
	void set_thumbnail(string thumbnail);
	string get_thumbnail();

private:
	int m_blog_id;			// 博客id
	string m_blog_title;	// 博客标题	
	string m_bolg_content;	// 博客内容
	int m_user_id;			// 用户id
	string m_bolg_postTime;			// 博客发布时间
	int m_category_id;		// 分类id
	string m_updatedAt;		// 博客更新时间
	int views;				// 浏览量
	string thumbnail;		// 封面路径

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

// 博客分类类
class Categories{
public:
	void set_id(int id);
	int get_id();
	void set_name(string name);
	string get_name();
	void set_description(string description);
	string get_description();
	void set_created_at(string created_at);
	string get_created_at();
	void set_updated_at(string updated_at);
	string get_updated_at();
	void set_blog_count(int blog_count);
	int get_blog_count();

private:
	int m_id;
	string m_name;
	string m_description;
	string m_created_at;
	string m_updated_at;
	int blog_count;
};

// 标签类
class Tags{
public:
	void set_id(int id);
	int get_id();
	void set_name(string name);
	string get_name();
	void set_description(string description);
	string get_description();
	void set_created_at(string created_at);
	string get_created_at();
	void set_updated_at(string updated_at);
	string get_updated_at();

private:
	int m_id;
	string m_name;
	string m_description;
	string m_created_at;
	string m_updated_at;
};

// 博客标签关联类
class Blog_tags{
public:
	void set_blog_id(int blog_id);
	int get_blog_id();
	void set_tag_id(int tag_id);
	int get_tag_id();

private:
	int m_blog_id;	// 博客id
	int m_tag_id;	// 标签id
};

// 对blog和user表进行数据库操作
class sql_blog_tool{
public:
	vector<Blog> select_all_blog();						// 查询所有博客
	vector<Blog> get_blogs_by_page(int page, int size);	// 分页查询博客
	vector<Blog> get_blogs_by_page_by_views(int page, int size);	// 分页查询博客，按浏览量排序	
	vector<Blog> get_blogs_by_category_id(int categoryId, int page, int size);	// 按分类id查询博客
	vector<Blog> get_blogs_by_tag_id(int tagId, int page, int size);	// 按标签id查询博客
	vector<Blog> get_blogs_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder);								// 也是分页查询，但是多了查询参数
	vector<Blog> get_blogs_by_category_and_page(int categoryId, int page, int size, const string& sortField, const string& sortOrder);				// 也是分页查询，但是多了参数
	vector<Blog> get_blogs_by_search(int page, int size, const string& sortField, const string& sortOrder, const string& keyword);	// 按关键词搜索博客
	vector<Blog> get_blogs_by_category_and_search(int categoryId, const string& keyword, int page, int size, const string& sortField, const string& sortOrder);
	vector<Comments> get_comments_by_search(const string& searchKeyword, int page, int size);// 按关键词搜索评论
	vector<Comments> get_comments(int page, int size);// 分页获取评论
	vector<Comments> get_comments_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder);
	vector<Comments> get_comments_by_page_and_sort_and_search(int page, int size, const string& sortField, const string& sortOrder, const string& searchKeyword);
	vector<User> get_users_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder);// 分页+筛选排序
	vector<User> get_users_by_page_and_sort_and_status(int page, int size, const string& sortField, const string& sortOrder, const string& status);// 分页+筛选排序
	vector<User> get_users_by_page_and_sort_and_search(int page, int size, const string& sortField, const string& sortOrder, const string& search);// 分页+筛选排序
	vector<User> get_users_by_page_and_sort_and_status_search(int page, int size, const string& sortField, const string& sortOrder,const string& status, const string& search);// 分页+筛选排序
	vector<Categories> get_categories();	// 获取所有分类
	vector<Categories> get_categories_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder);// 分页+排序
	vector<Categories> get_categories_by_page_and_sort_and_search(int page, int size, const string& sortField, const string& sortOrder, const string& search);// 分页+排序+搜索
	vector<Categories> get_categories_by_articles_count(int page, int size, const string& sortOrder);
	vector<Categories> get_categories_by_articles_count_and_search(int page, int size, const string& sortOrder, const string& searchKeyword);
	vector<string> get_all_tags();	// 获取所有标签
	vector<Tags> get_tags_by_blog_count(int page, int size, const string& sortOrder);	// 获取标签按博客数量排序
	vector<Tags> get_tags_by_blog_count_and_search(int page, int size, const string& sortOrder, const string& searchKeyword);	// 获取标签按博客数量排序+搜索
	vector<Tags> get_tags_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder);	// 获取标签按指定字段排序
	vector<Tags> get_tags_by_page_and_sort_and_search(int page, int size, const string& sortField, const string& sortOrder, const string& searchKeyword);	// 获取标签按指定字段排序+搜索
	int get_total_blog_count();							// 获取博客的总条数
	int get_total_blog_count_by_category(int categoryId);// 根据分类获取博客的总条数
	int get_total_blog_count_by_search(const string& keyword);	// 获取符合搜索条件的博客总数
	int get_total_comments_count_by_search(const string& keyword);	// 获取符合搜索条件的评论总数
	int get_total_categories_count_by_search(const string& keyword);	// 获取符合搜索条件的分类总数
	int get_total_blog_count_by_category_and_search(int categoryId, const string& keyword);
	int get_user_count();								// 获取用户的总数
	int get_user_count_by_status(const string& status);		// 根据用户状态获取用户总数
	int get_user_count_by_search(string search);		// 根据搜索获取用户总数
	int get_user_count_by_status_search(string status, string search);		// 根据搜索获取用户总数
	int get_categorie_count();							// 获取分类总数
	int get_comment_count();							// 获取评论总数
	int get_like_count();								// 获取点赞总数
	int get_tag_count();								// 获取标签总数
	int get_total_tags_count_by_search(const string& keyword);				// 获取符合搜索条件的标签总数
	int get_total_blog_count_by_tag(int tagid);		// 获取指定标签的博客总数
	Blog select_blog_by_id(int blogid);					// 通过博客id查询博客内容
	Blog get_prev_blog_by_id(int blogid);				// 通过博客id获取上一篇博客
	Blog get_next_blog_by_id(int blogid);				// 通过博客id获取下一篇博客
	vector<Blog> get_related_blogs(int category, int excludeId, int size);	// 获取相关文章
	int get_userid_by_blogid(int blogid);				// 通过博客id获取对应的用户id
	void modify_blog_by_blogid(Blog blog);				// 通过博客id修改博客的标题和内容
	bool delete_blog_by_blogid(int blogid);				// 通过博客id删除指定博客
	vector<Comments> get_comments_by_blogid(int blogid);	// 通过博客id获取对应的评论内容
	void add_comment_by_blogid(Comments comment);		// 通过博客id插入评论
	User get_userdata_by_userid(int userid); 			// 通过用户id获取用户信息
	vector<Blog> get_blogs_by_userid(int userid);		// 通过用户id获取该用户的所有博客
	int get_article_count_by_userid(int userid);		// 通过用户id获取该用户的文章总数
	vector<Messages> get_messages_by_userid(int userid);// 通过用户id获取该用户收到的所有信息
	bool insert_blog(Blog blog);						// 将用户post过来的博客内容存储数据库
	int add_blog(string title, string content, int userid, int categoryid, string thumbnail_path);	// 添加博客
	int get_userid(string username);					// 通过用户名获取用户id
	void modify_password_by_username(string username, string password);	// 通过用户名修改用户密码
	bool check_username_is_exist(string username);		// 检查该用户名是否已经被注册

	bool update_avatar_path(string username, string file_path);	// 存储用户头像路径

	bool update_last_login_time(int user_id);	// 更新用户最新登录时间

	// 个人中心-消息中心相关
	bool check_message_belongs_to_user(int userid, int messageid);	// 检查消息是否属于该用户
	bool mark_message_as_read(int messageid);			// 标记消息为已读
	bool mark_all_message_as_read(int userid);			// 标记所有消息为已读
	bool delete_message(int messageid);					// 删除指定消息
	bool insert_new_message(Messages message);			// 插入新消息

	// 博客点赞相关
	
	bool insert_new_blog_like(int userid, int blogid);		// 插入新的博客点赞
	bool remove_blog_like(int userid, int blogid);		// 删除博客点赞
	int get_blog_likes_count(int blogid);				// 获取当前博客的点赞总数
	bool is_user_liked_blog(int userid, int blog_id);	// 检测用户是否已经对该博客点赞
	int get_view_count_by_userid(int userid);			// 获取用户浏览量
	int get_blog_liked_count_by_userid(int userid);		// 获取用户获赞总数
	// 博客评论相关
	int get_blog_comments_count(int blogid);			// 获取当前博客的评论总数
	bool delete_comment_by_commentid(int commentid);	// 删除指定id的评论
	bool batch_delete_comments(const vector<int> &commentIds);// 批量删除评论
	Comments get_comment_by_commentId(int commentId);	// 通过评论id获取评论详情
	bool update_comment_by_commentid(int commentid, Comments comment);
	int add_comment_to_article(string username, int articleid, string content);	// 添加评论到文章

	bool increase_blog_view_count(int blogid);		// 增加博客的浏览量（+1）
	bool increase_article_count(int user_id);		// 增加文章总数（+1）
	bool decrease_article_count(int user_id);		// 文章总数-1


	// 管理员操作
	bool user_register(string username, string password, string email, string avatar);		// 用户注册
	bool add_user_from_admin(User user);		// 管理员添加用户
	bool delete_user_by_userid(int userid);		// 删除用户
	bool update_user_by_userid(int userid, User user);	// 更新用户信息
	bool update_blog_by_blogid(int blogid, Blog blog);	// 更新文章信息
	string get_admin_password_by_username(string useranme);	// 根据管理员昵称获取对应的密码
	vector<Categories> get_cotegories_all();			// 获取所有的分类
	string get_cotegoriename_by_cotegorieid(int cotegorieid);	// 根据分类id获取分类名称
	Categories get_categorie_by_categorieid(int categorieId);	// 根据分类id获取分类信息
	int get_category_id_by_name(const string& categorieName);	// 根据分类名称获取分类id
	Categories get_categorie_by_name(const string& categorieName);	// 根据分类名称获取分类信息
	bool add_categorie(Categories categorie);	// 添加分类
	bool add_tag(Tags tag);					// 添加标签
	bool update_categorie_by_categorieid(int categorieid, Categories categorie);
	bool delete_categorie_by_categorieid(int categorieid);
	bool delete_tag_by_tagid(int tagid);		// 删除标签
	vector<string> get_tags_by_blogid(int blogid);		// 通过博客id获取该博客的标签

	bool delete_blog_tags(int blogid);	// 根据博客id删除旧的标签关联
	int get_tag_id_by_name(string tag); // 根据标签名称获取标签id
	Tags get_tag_by_tagname(const string& tagname);	// 根据标签名称获取标签信息
	int create_tag(string tag);
	bool add_blog_tag(int blogid, int tagid);

public:
	int m_close_log;	// 日志开关
};

#endif