#include "sql_tool.h"
#include "sql_connection_pool.h"

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

// 更新博客内容
void sql_blog_tool::modify_blog_by_blogid(Blog blog)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句：修改博客内容
    const char* query = "UPDATE blog SET title = ?, content = ? WHERE blogId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 获取Blog对象的数据
    string title = blog.get_blog_title();
    string content = blog.get_blog_content();
    int blogId = blog.get_blog_id();  // 获取博客 ID，用于更新指定的博客

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    // 绑定每个字段的数据
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)title.c_str();
    bind[0].buffer_length = title.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)content.c_str();
    bind[1].buffer_length = content.size();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (char*)&blogId;

    // 将绑定的数据应用到语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
    } else {
        cout << "Blog updated successfully" << endl;
    }

    // 关闭语句
    mysql_stmt_close(stmt);
}

// 通过博客id删除指定博客
void sql_blog_tool::delete_blog_by_blogid(int blogid)
{
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 删除博客的 SQL 查询
    const char* query = "DELETE FROM blog WHERE blogId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 绑定查询参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 执行 SQL 语句（删除操作）
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 判断删除是否成功
    if (mysql_stmt_affected_rows(stmt) > 0) {
        cout << "博客 ID " << blogid << " 删除成功!" << endl;
    } else {
        cout << "未找到博客 ID " << blogid << "，删除失败!" << endl;
    }

    // 关闭 SQL 语句
    mysql_stmt_close(stmt);
}

// 通过博客id获取对应的评论内容
vector<Comments> sql_blog_tool::get_comments_by_blogid(int blogid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII msqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句
    const char* query = "SELECT username, content, comment_time FROM blog_comments WHERE blog_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return {};
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return {};
    }

    // 绑定参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return {};
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return {};
    }

    // 获取结果元信息
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return {};
    }

    // 绑定结果
    MYSQL_BIND bind_result[3];
    memset(bind_result, 0, sizeof(bind_result));

    char username[256];
    char content[1024];
    char comment_time[20];

    bind_result[0].buffer_type = MYSQL_TYPE_STRING;
    bind_result[0].buffer = username;
    bind_result[0].buffer_length = sizeof(username);

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = content;
    bind_result[1].buffer_length = sizeof(content);

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = comment_time;
    bind_result[2].buffer_length = sizeof(comment_time);

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_free_result(prepare_meta_result);
        mysql_stmt_close(stmt);
        return {};
    }

    // 获取结果并存储到 vector
    vector<Comments> comments_list;

    while (mysql_stmt_fetch(stmt) == 0) {
        Comments comment;
        comment.set_username(string(username));
        comment.set_content(string(content));
        comment.set_comment_time(string(comment_time));
        comments_list.push_back(comment);
    }

    // 释放资源
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return comments_list;
}

// 通过博客id插入评论
void sql_blog_tool::add_comment_by_blogid(Comments comment)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII msqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 插入语句
    const char* query = "INSERT INTO blog_comments (blog_id, username, content, comment_time) VALUES (?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 绑定参数
    MYSQL_BIND bind_param[4];
    memset(bind_param, 0, sizeof(bind_param));

    int blog_id = comment.get_blog_id();
    const string& username = comment.get_username();
    const string& content = comment.get_content();
    const string& comment_time = comment.get_comment_time();

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blog_id;

    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = (char*)username.c_str();
    bind_param[1].buffer_length = username.length();

    bind_param[2].buffer_type = MYSQL_TYPE_STRING;
    bind_param[2].buffer = (char*)content.c_str();
    bind_param[2].buffer_length = content.length();

    bind_param[3].buffer_type = MYSQL_TYPE_STRING;
    bind_param[3].buffer = (char*)comment_time.c_str();
    bind_param[3].buffer_length = comment_time.length();

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 打印插入成功信息（可选）
    cout << "Comment successfully added for blog ID: " << blog_id << endl;

    // 释放资源
    mysql_stmt_close(stmt);
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
    const char* query = "SELECT userId, username, password, avatar, article_count, register_time, email, description FROM user WHERE userId = ?";
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
    MYSQL_BIND bind_result[8];
    memset(bind_result, 0, sizeof(bind_result));

    int userId;
    char username[128];
    char password[128];
    char avatar[256];
    int article_count;
    char register_time[64];
    char email[128];
    char description[256];

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

    bind_result[7].buffer_type = MYSQL_TYPE_STRING;
    bind_result[7].buffer = (char*)description;
    bind_result[7].buffer_length = sizeof(description);

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
        user.set_description(description);

        mysql_stmt_close(stmt);
        return user;
    } else {
        cerr << "mysql_stmt_fetch() failed or no result found" << endl;
        mysql_stmt_close(stmt);
        return User(); // 返回空用户对象
    }
}

// 通过用户id获取该用户的所有博客
vector<Blog> sql_blog_tool::get_blogs_by_userid(int userid)
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
	const char* query = "SELECT blogId, title, content, userId, postTime FROM blog WHERE userId = ? ORDER BY postTime DESC";

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

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&userid;

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

// 通过用户id获取该用户收到的所有信息
vector<Messages> sql_blog_tool::get_messages_by_userid(int userid)
{
    vector<Messages> messages;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句
    const char* query = "SELECT message_id, type, content, post_time, is_read, blog_id FROM messages WHERE recipient_id = ? ORDER BY post_time DESC";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return messages;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return messages;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &userid;  // 直接传递地址

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return messages;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return messages;
    }

    // 获取结果
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return messages;
    }

    int message_id;
    char type[32];
    char content[1024];
    char postTime[32];
    my_bool is_read;  // 修改为 my_bool
    int blog_id;

    MYSQL_BIND result_bind[6];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &message_id;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = type;
    result_bind[1].buffer_length = sizeof(type);
    result_bind[1].is_null = 0;
    result_bind[1].length = 0;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = content;
    result_bind[2].buffer_length = sizeof(content);
    result_bind[2].is_null = 0;
    result_bind[2].length = 0;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = postTime;
    result_bind[3].buffer_length = sizeof(postTime);
    result_bind[3].is_null = 0;
    result_bind[3].length = 0;

    result_bind[4].buffer_type = MYSQL_TYPE_TINY;
    result_bind[4].buffer = &is_read;
    result_bind[4].is_null = 0;
    result_bind[4].length = 0;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = &blog_id;
    result_bind[5].is_null = 0;
    result_bind[5].length = 0;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return messages;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {  // 修改判断条件
        Messages message;
        message.set_message_id(message_id);
        message.set_type(type);
        message.set_content(content);
        message.set_post_time(postTime);
        message.set_is_read(is_read);
        message.set_blog_id(blog_id);
        messages.push_back(message);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return messages;
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

// 通过用户名修改用户密码
void sql_blog_tool::modify_password_by_username(string username, string password) {
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 设置字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 查询语句
    const char* query = "UPDATE user SET password = ? WHERE username = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param[2];
    memset(bind_param, 0, sizeof(bind_param));

    // 绑定密码参数
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = (char*)password.c_str();
    bind_param[0].buffer_length = password.length();

    // 绑定用户名参数
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = (char*)username.c_str();
    bind_param[1].buffer_length = username.length();

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
    } else {
        cout << "密码修改成功！" << endl;
    }

    // 清理
    mysql_stmt_close(stmt);
}

// 检查消息是否属于该用户
bool sql_blog_tool::check_message_belongs_to_user(int userid, int messageid)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建SQL查询语句
    string query = "SELECT COUNT(*) FROM messages WHERE message_id = ? AND recipient_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return false;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param[2];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&messageid;
    bind_param[0].is_null = 0;

    bind_param[1].buffer_type = MYSQL_TYPE_LONG;
    bind_param[1].buffer = (char*)&userid;
    bind_param[1].is_null = 0;

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 获取结果
    int count = 0;
    MYSQL_BIND bind_result;
    memset(&bind_result, 0, sizeof(bind_result));
    bind_result.buffer_type = MYSQL_TYPE_LONG;
    bind_result.buffer = (char*)&count;

    if (mysql_stmt_bind_result(stmt, &bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return count > 0;
}

// 标记消息为已读
bool sql_blog_tool::mark_message_as_read(int messageid)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建SQL更新语句
    string query = "UPDATE messages SET is_read = 1 WHERE message_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return false;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));
    bind_param.buffer_type = MYSQL_TYPE_LONG;
    bind_param.buffer = (char*)&messageid;

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, &bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行更新语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);
    return true;
}

// 标记所有消息为已读
bool sql_blog_tool::mark_all_message_as_read(int userid)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建SQL更新语句
    string query = "UPDATE messages SET is_read = 1 WHERE recipient_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return false;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));
    bind_param.buffer_type = MYSQL_TYPE_LONG;
    bind_param.buffer = (char*)&userid;

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, &bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行更新语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);
    return true;
}

// 删除指定消息
bool sql_blog_tool::delete_message(int messageid)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建SQL删除语句
    string query = "DELETE FROM messages WHERE message_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return false;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));
    bind_param.buffer_type = MYSQL_TYPE_LONG;
    bind_param.buffer = (char*)&messageid;

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, &bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行删除语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);
    return true;
}

// 插入新消息
bool sql_blog_tool::insert_new_message(Messages message)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中获取一个连接
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句
    const char* query = "INSERT INTO messages (sender_id, blog_id, type, content, post_time, is_read, recipient_id) VALUES (?, ?, ?, ?, ?, ?, ?)";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    // 准备SQL语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }


    // 绑定参数
    MYSQL_BIND bind[7];
    memset(bind, 0, sizeof(bind));

    int sender_id = message.get_sender_id();
    int blog_id = message.get_blog_id();
    int recipient_id = message.get_recipient_id();

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&sender_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&blog_id;
    bind[1].is_null = 0;
    bind[1].length = 0;

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)message.get_type().c_str();
    bind[2].buffer_length = message.get_type().length();
    bind[2].is_null = 0;
    bind[2].length = 0;

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)message.get_content().c_str();
    bind[3].buffer_length = message.get_content().length();
    bind[3].is_null = 0;
    bind[3].length = 0;

    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = (char*)message.get_post_time().c_str();
    bind[4].buffer_length = message.get_post_time().length();
    bind[4].is_null = 0;
    bind[4].length = 0;

    my_bool is_read = message.get_is_read();
    bind[5].buffer_type = MYSQL_TYPE_TINY;
    bind[5].buffer = &is_read;
    bind[5].is_null = 0;
    bind[5].length = 0;

    bind[6].buffer_type = MYSQL_TYPE_LONG;
    bind[6].buffer = (char*)&recipient_id;
    bind[6].is_null = 0;
    bind[6].length = 0;

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行插入操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return true;
}


/*
 *下面的内容为用户类
*/

void User::set_userid(int userid)
{
    this->m_userId = userid;
}

int User::get_userid()
{
    return this->m_userId;
}

void User::set_username(string username){
	this->m_username = username;
}

string User::get_username()
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

void User::set_description(string description)
{
    this->description = description;
}

string User::get_description()
{
    return this->description;
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


/*
 *下面的内容为评论类
*/
void Comments::set_comment_id(int comment_id)
{
    this->m_comment_id = comment_id;
}

int Comments::get_comment_id()
{
    return this->m_comment_id;
}

void Comments::set_blog_id(int blog_id)
{
    this->m_blog_id = blog_id;
}

int Comments::get_blog_id()
{
    return this->m_blog_id;
}

void Comments::set_username(string username)
{
    this->m_username = username;
}

string Comments::get_username()
{
    return this->m_username;
}

void Comments::set_content(string content)
{
    this->m_content = content;
}

string Comments::get_content()
{
    return this->m_content;
}

void Comments::set_parent_id(int parent_id)
{
    this->m_parent_id = parent_id;
}

int Comments::get_parent_id()
{
    return this->m_parent_id;
}

void Comments::set_comment_time(string comment_time)
{
    this->m_comment_time = comment_time;
}

string Comments::get_comment_time()
{
    return this->m_comment_time;
}

/*
 *下面的内容为消息类
*/
void Messages::set_message_id(int message_id)
{
    this->m_message_id = message_id;
}

int Messages::get_message_id()
{
    return this->m_message_id;
}

void Messages::set_sender_id(int sender_id)
{
    this->m_sender_id = sender_id;
}

int Messages::get_sender_id()
{
    return this->m_sender_id;
}

void Messages::set_recipient_id(int recipient_id)
{
    this->m_recipient_id = recipient_id;
}

int Messages::get_recipient_id()
{
    return this->m_recipient_id;
}

void Messages::set_blog_id(int blog_id)
{
    this->m_blog_id = blog_id;
}

int Messages::get_blog_id()
{
    return this->m_blog_id;
}

void Messages::set_type(string type)
{
    this->m_type = type;
}

string Messages::get_type()
{
    return this->m_type;
}

void Messages::set_content(string content)
{
    this->m_content = content;
}

string Messages::get_content()
{
    return this->m_content;
}

void Messages::set_post_time(string post_time)
{
    this->m_post_time = post_time;
}

string Messages::get_post_time()
{
    return this->m_post_time;
}

void Messages::set_is_read(bool is_read)
{
    this->m_is_read = is_read;
}

bool Messages::get_is_read()
{
    return this->m_is_read;
}
