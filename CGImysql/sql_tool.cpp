#include "sql_tool.h"
#include "sql_connection_pool.h"
#include <algorithm>

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

    // 预处理SQL语句，只获取内容的前200个字符作为摘要
    const char* query = "SELECT blogId, title, LEFT(content, 200) AS content, userId, postTime, category_id, updatedAt, view_count, thumbnail FROM blog ORDER BY postTime DESC LIMIT ? OFFSET ?";

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
    char content[1024];  // 由于现在只获取前200个字符，这个缓冲区足够了
    int userId;
    char postTime[32];
    int category_id;
    char updatedAt[32];
    int view_count;
    char thumbnail[256];
    
    // 添加长度指示器变量
    unsigned long title_length;
    unsigned long content_length;
    unsigned long postTime_length;
    unsigned long updatedAt_length;
    unsigned long thumbnail_length;

    MYSQL_BIND result_bind[9];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&blogId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = (char*)title;
    result_bind[1].buffer_length = sizeof(title);
    result_bind[1].length = &title_length;  // 添加长度指示器

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = (char*)content;
    result_bind[2].buffer_length = sizeof(content);
    result_bind[2].length = &content_length;  // 添加长度指示器

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = (char*)&userId;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = (char*)postTime;
    result_bind[4].buffer_length = sizeof(postTime);
    result_bind[4].length = &postTime_length;  // 添加长度指示器

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = (char*)updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);
    result_bind[6].length = &updatedAt_length;  // 添加长度指示器

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

    result_bind[8].buffer_type = MYSQL_TYPE_STRING;
    result_bind[8].buffer = thumbnail;
    result_bind[8].buffer_length = sizeof(thumbnail);
    result_bind[8].length = &thumbnail_length;

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
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt));
        blog.set_views(view_count);
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 分页查询博客，按浏览量排序	
vector<Blog> sql_blog_tool::get_blogs_by_page_by_views(int page, int size)
{
    vector<Blog> blogs;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句,添加thumbnail字段
    const char* query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count, thumbnail FROM blog ORDER BY view_count DESC LIMIT ? OFFSET ?";

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

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 定义变量存储结果
    int blogId;
    char title[256];
    char content[65535];  // 增大content缓冲区大小
    int userId;
    char postTime[32];
    int category_id;
    char updatedAt[32];
    int view_count;
    char thumbnail[256];

    // 定义长度变量
    unsigned long title_length, content_length, postTime_length, updatedAt_length, thumbnail_length;

    MYSQL_BIND result_bind[9];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&blogId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = title;
    result_bind[1].buffer_length = sizeof(title);
    result_bind[1].length = &title_length;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = content;
    result_bind[2].buffer_length = sizeof(content);
    result_bind[2].length = &content_length;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = (char*)&userId;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = postTime;
    result_bind[4].buffer_length = sizeof(postTime);
    result_bind[4].length = &postTime_length;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);
    result_bind[6].length = &updatedAt_length;

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

    result_bind[8].buffer_type = MYSQL_TYPE_STRING;
    result_bind[8].buffer = thumbnail;
    result_bind[8].buffer_length = sizeof(thumbnail);
    result_bind[8].length = &thumbnail_length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_free_result(prepare_meta_result);
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Blog blog;
        blog.set_blog_id(blogId);
        blog.set_blog_title(string(title, title_length));
        blog.set_blog_content(string(content, content_length));
        blog.set_user_id(userId);
        blog.set_blog_postTime(string(postTime, postTime_length));
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt, updatedAt_length));
        blog.set_views(view_count);
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
        blogs.push_back(blog);
    }

    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 按分类id查询博客
vector<Blog> sql_blog_tool::get_blogs_by_category_id(int categoryId, int page, int size)
{
    vector<Blog> blogs;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造SQL查询语句
    string query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count, thumbnail FROM blog WHERE category_id = ? ORDER BY postTime DESC LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 计算偏移量
    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&categoryId;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&size;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (char*)&offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 执行查询
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

    // 定义变量存储结果
    int blogId;
    char title[256];
    char content[65535];
    int userId;
    char postTime[32];
    int category_id;
    char updatedAt[32];
    int view_count;
    char thumbnail[256];

    // 定义长度变量
    unsigned long title_length, content_length, postTime_length, updatedAt_length, thumbnail_length;

    MYSQL_BIND result_bind[9];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&blogId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = title;
    result_bind[1].buffer_length = sizeof(title);
    result_bind[1].length = &title_length;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = content;
    result_bind[2].buffer_length = sizeof(content);
    result_bind[2].length = &content_length;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = (char*)&userId;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = postTime;
    result_bind[4].buffer_length = sizeof(postTime);
    result_bind[4].length = &postTime_length;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING; 
    result_bind[6].buffer = updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);
    result_bind[6].length = &updatedAt_length;

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count; 

    result_bind[8].buffer_type = MYSQL_TYPE_STRING;
    result_bind[8].buffer = thumbnail;
    result_bind[8].buffer_length = sizeof(thumbnail);
    result_bind[8].length = &thumbnail_length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_free_result(prepare_meta_result);
        mysql_stmt_close(stmt);
        return blogs;   
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Blog blog;
        blog.set_blog_id(blogId);   
        blog.set_blog_title(string(title, title_length));
        blog.set_blog_content(string(content, content_length));
        blog.set_user_id(userId);
        blog.set_blog_postTime(string(postTime, postTime_length));
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt, updatedAt_length));    
        blog.set_views(view_count);
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result); 
    mysql_stmt_close(stmt);

    return blogs;
}

// 按标签id查询博客
vector<Blog> sql_blog_tool::get_blogs_by_tag_id(int tagId, int page, int size)
{
    vector<Blog> blogs;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造SQL查询语句 - 使用JOIN连接blog和blog_tags表
    string query = "SELECT b.blogId, b.title, b.content, b.userId, b.postTime, b.category_id, b.updatedAt, b.view_count, b.thumbnail "
                   "FROM blog b "
                   "INNER JOIN blog_tags bt ON b.blogId = bt.blog_id "
                   "WHERE bt.tag_id = ? "
                   "ORDER BY b.postTime DESC LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);  
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {  
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 计算偏移量   
    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));  

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&tagId;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&size;  

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (char*)&offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;   
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 执行查询
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

    // 定义变量存储结果
    int blogId;
    char title[256];
    char content[65535];
    int userId; 
    char postTime[32];      
    int category_id;
    char updatedAt[32];
    int view_count;
    char thumbnail[256];

    // 定义长度变量
    unsigned long title_length, content_length, postTime_length, updatedAt_length, thumbnail_length;

    MYSQL_BIND result_bind[9];
    memset(result_bind, 0, sizeof(result_bind));    
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&blogId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = title;
    result_bind[1].buffer_length = sizeof(title);
    result_bind[1].length = &title_length;  

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = content;
    result_bind[2].buffer_length = sizeof(content);
    result_bind[2].length = &content_length;    

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = (char*)&userId;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = postTime;   
    result_bind[4].buffer_length = sizeof(postTime);
    result_bind[4].length = &postTime_length;

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;    

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);
    result_bind[6].length = &updatedAt_length;          

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

    result_bind[8].buffer_type = MYSQL_TYPE_STRING;
    result_bind[8].buffer = thumbnail;
    result_bind[8].buffer_length = sizeof(thumbnail);
    result_bind[8].length = &thumbnail_length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_free_result(prepare_meta_result);
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Blog blog;
        blog.set_blog_id(blogId);
        blog.set_blog_title(string(title, title_length));
        blog.set_blog_content(string(content, content_length));
        blog.set_user_id(userId);
        blog.set_blog_postTime(string(postTime, postTime_length));
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt, updatedAt_length));
        blog.set_views(view_count);
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result); 
    mysql_stmt_close(stmt);

    return blogs;
}

// 也是分页查询博客列表，但是多了查询参数
vector<Blog> sql_blog_tool::get_blogs_by_page_and_sort(int page, int size, const string& sortField, const string& sortOrder)
{
    // 用于存储查询到的博客
    vector<Blog> blogs;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造动态 SQL 查询，加入排序字段和排序方式
    string query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count FROM blog ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    const char* query_cstr = query.c_str();

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query_cstr, strlen(query_cstr))) {
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
    int category_id;
    char updatedAt[32];
    int view_count;

    MYSQL_BIND result_bind[8];
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

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = (char*)updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

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
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt));
        blog.set_views(view_count);
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 也是分页查询博客列表，但是多了参数
vector<Blog> sql_blog_tool::get_blogs_by_category_and_page(int categoryId, int page, int size, const string& sortField, const string& sortOrder)
{
    // 用于存储查询到的博客
    vector<Blog> blogs;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造动态 SQL 查询，加入分类筛选和排序
    string query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count FROM blog WHERE category_id = ? ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";
    const char* query_cstr = query.c_str();

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query_cstr, strlen(query_cstr))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 计算偏移量
    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&categoryId;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&size;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (char*)&offset;

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
    int category_id;
    char updatedAt[32];
    int view_count;

    MYSQL_BIND result_bind[8];
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

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = (char*)updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

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
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt));
        blog.set_views(view_count);
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 按关键词搜索博客
vector<Blog> sql_blog_tool::get_blogs_by_search(int page, int size, const string& sortField, const string& sortOrder, const string& keyword) {
    vector<Blog> blogs;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    string query = "SELECT * FROM blog WHERE title LIKE ? OR content LIKE ? ORDER BY " + sortField + " " + sortOrder + " LIMIT ?, ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return blogs;

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return blogs;
    }

    string searchPattern = "%" + keyword + "%";
    int offset = (page - 1) * size;

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &offset;
    bind[2].is_null = 0;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &size;
    bind[3].is_null = 0;

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
    int category_id;
    char updatedAt[32];
    int view_count;

    MYSQL_BIND result_bind[8];
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

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = (char*)updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

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
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt));
        blog.set_views(view_count);
        blogs.push_back(blog);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

vector<Blog> sql_blog_tool::get_blogs_by_category_and_search(int categoryId, const string& keyword, int page, int size, const string& sortField, const string& sortOrder)
{
    vector<Blog> blogs;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // SQL 语句，增加搜索功能
    string query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count "
                   "FROM blog "
                   "WHERE category_id = ? AND (title LIKE ? OR content LIKE ?) "
                   "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";
    const char* query_cstr = query.c_str();

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return blogs;
    }

    if (mysql_stmt_prepare(stmt, query_cstr, strlen(query_cstr))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    // 计算分页偏移量
    int offset = (page - 1) * size;
    string searchPattern = "%" + keyword + "%"; // 关键字前后加 %

    // 绑定参数
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&categoryId;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)searchPattern.c_str();
    bind[2].buffer_length = searchPattern.length();

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = (char*)&size;

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = (char*)&offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    int blogId, userId, category_id, view_count;
    char title[256], content[1024], postTime[32], updatedAt[32];

    MYSQL_BIND result_bind[8];
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

    result_bind[5].buffer_type = MYSQL_TYPE_LONG;
    result_bind[5].buffer = (char*)&category_id;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = (char*)updatedAt;
    result_bind[6].buffer_length = sizeof(updatedAt);

    result_bind[7].buffer_type = MYSQL_TYPE_LONG;
    result_bind[7].buffer = (char*)&view_count;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return blogs;
    }

    while (!mysql_stmt_fetch(stmt)) {
        Blog blog;
        blog.set_blog_id(blogId);
        blog.set_blog_title(string(title));
        blog.set_blog_content(string(content));
        blog.set_user_id(userId);
        blog.set_blog_postTime(string(postTime));
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updatedAt));
        blog.set_views(view_count);
        blogs.push_back(blog);
    }

    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return blogs;
}

// 按关键词搜索评论
vector<Comments> sql_blog_tool::get_comments_by_search(const string &searchKeyword, int page, int size)
{
    vector<Comments> comments;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    string query = "SELECT comment_id, blog_id, username, content, comment_time FROM blog_comments WHERE username LIKE ? OR content LIKE ? ORDER BY comment_time DESC LIMIT ?, ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) return comments;

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        mysql_stmt_close(stmt);
        return comments;
    }

    string searchPattern = "%" + searchKeyword + "%";
    int offset = (page - 1) * size;

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &offset;
    bind[2].is_null = 0;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &size;
    bind[3].is_null = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 获取结果
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    int commentId;
    int blogId;
    char username[256];
    char content[1024];
    char postTime[32];

    MYSQL_BIND result_bind[5];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &commentId;
    result_bind[0].buffer_length = sizeof(commentId);


    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = (char*)&blogId;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = (char*)username;
    result_bind[2].buffer_length = sizeof(username);

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = (char*)content;
    result_bind[3].buffer_length = sizeof(content);

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = (char*)postTime;
    result_bind[4].buffer_length = sizeof(postTime);

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Comments comment;
        comment.set_comment_id(commentId);
        comment.set_blog_id(blogId);
        comment.set_username(username);
        comment.set_content(content);
        comment.set_comment_time(postTime);
        comments.push_back(comment);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return comments;
}

// 分页获取评论
vector<Comments> sql_blog_tool::get_comments(int page, int size)
{
    // 用于存储查询到的博客
	vector<Comments> comments;

	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;

	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 预处理SQL语句
	const char* query = "SELECT comment_id, blog_id, username, content, comment_time FROM blog_comments ORDER BY comment_time DESC LIMIT ? OFFSET ?";

	MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return comments;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
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
        return comments;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 获取结果
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    int commentId;
    int blogId;
    char username[256];
    char content[1024];
    char postTime[32];

    MYSQL_BIND result_bind[5];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&commentId;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = (char*)&blogId;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = (char*)username;
    result_bind[2].buffer_length = sizeof(username);

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = (char*)content;
    result_bind[3].buffer_length = sizeof(content);

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = (char*)postTime;
    result_bind[4].buffer_length = sizeof(postTime);

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 遍历结果集
    while (!mysql_stmt_fetch(stmt)) {
        Comments comment;
        comment.set_comment_id(commentId);
        comment.set_blog_id(blogId);
        comment.set_username(username);
        comment.set_content(content);
        comment.set_comment_time(postTime);
        comments.push_back(comment);
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return comments;
}

vector<Comments> sql_blog_tool::get_comments_by_page_and_sort(int page, int size, const string &sortField, const string &sortOrder)
{
    // 用于存储查询到的评论
    vector<Comments> comments;

    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造安全的 SQL 语句（字段名不能用 ? 绑定参数，因此只能拼接）
    string query = "SELECT comment_id, blog_id, username, content, comment_time FROM blog_comments ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return comments;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 计算偏移量
    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &size;
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 结果绑定
    int commentId, blogId;
    char username[256] = {};
    char content[1024] = {};
    char comment_time[32] = {};
    unsigned long username_length, content_length, comment_time_length;
    my_bool username_is_null, content_is_null, comment_time_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &commentId;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &blogId;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = username;
    result_bind[2].buffer_length = sizeof(username);
    result_bind[2].length = &username_length;
    result_bind[2].is_null = &username_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content;
    result_bind[3].buffer_length = sizeof(content);
    result_bind[3].length = &content_length;
    result_bind[3].is_null = &content_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = comment_time;
    result_bind[4].buffer_length = sizeof(comment_time);
    result_bind[4].length = &comment_time_length;
    result_bind[4].is_null = &comment_time_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Comments comment;
        comment.set_comment_id(commentId);
        comment.set_blog_id(blogId);
        
        if (!username_is_null) {
            comment.set_username(string(username, username_length));
        }
        if (!content_is_null) {
            comment.set_content(string(content, content_length));
        }
        if (!comment_time_is_null) {
            comment.set_comment_time(string(comment_time, comment_time_length));
        }

        comments.push_back(comment);
    }

    mysql_stmt_close(stmt);
    return comments;
}

vector<Comments> sql_blog_tool::get_comments_by_page_and_sort_and_search(int page, int size, const string &sortField, const string &sortOrder, const string &searchKeyword)
{
    vector<Comments> comments;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + searchKeyword + "%";

    // 构造安全的 SQL 语句
    string query = "SELECT comment_id, blog_id, username, content, comment_time "
                   "FROM blog_comments WHERE (username LIKE ? OR content LIKE ?) "
                   "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return comments;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[4] = {};
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &size;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 结果绑定
    int commentId, blogId;
    char username[256] = {};
    char content[1024] = {};
    char comment_time[32] = {};
    unsigned long username_length, content_length, comment_time_length;
    my_bool username_is_null, content_is_null, comment_time_is_null;

    MYSQL_BIND result_bind[5] = {};
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &commentId;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &blogId;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = username;
    result_bind[2].buffer_length = sizeof(username);
    result_bind[2].length = &username_length;
    result_bind[2].is_null = &username_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content;
    result_bind[3].buffer_length = sizeof(content);
    result_bind[3].length = &content_length;
    result_bind[3].is_null = &content_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = comment_time;
    result_bind[4].buffer_length = sizeof(comment_time);
    result_bind[4].length = &comment_time_length;
    result_bind[4].is_null = &comment_time_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comments;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Comments comment;
        comment.set_comment_id(commentId);
        comment.set_blog_id(blogId);
        
        if (!username_is_null) {
            comment.set_username(string(username, username_length));
        }
        if (!content_is_null) {
            comment.set_content(string(content, content_length));
        }
        if (!comment_time_is_null) {
            comment.set_comment_time(string(comment_time, comment_time_length));
        }

        comments.push_back(comment);
    }

    mysql_stmt_close(stmt);
    return comments;
}

// 分页+筛选排序
vector<User> sql_blog_tool::get_users_by_page_and_sort(int page, int size, const string &sortField, const string &sortOrder)
{
    vector<User> users;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造安全的 SQL 语句（字段名不能用 ? 绑定参数，因此只能拼接）
    string query = "SELECT userId, username, avatar, article_count, register_time, email, status, last_login_time FROM user ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return users;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &size;
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 结果绑定
    int userId, article_count;
    char username[128] = {};
    char avatar[256] = {};
    char register_time[64] = {};
    char last_login_time[64] = {};
    char email[128] = {};
    char status[16] = {};
    unsigned long username_length, avatar_length, register_time_length, email_length, status_length, last_login_time_length;
    my_bool username_is_null, avatar_is_null, register_time_is_null, email_is_null, status_is_null, last_login_time_is_null;

    MYSQL_BIND result_bind[8] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &userId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = username;
    result_bind[1].buffer_length = sizeof(username);
    result_bind[1].length = &username_length;
    result_bind[1].is_null = &username_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = avatar;
    result_bind[2].buffer_length = sizeof(avatar);
    result_bind[2].length = &avatar_length;
    result_bind[2].is_null = &avatar_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &article_count;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = register_time;
    result_bind[4].buffer_length = sizeof(register_time);
    result_bind[4].length = &register_time_length;
    result_bind[4].is_null = &register_time_is_null;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = email;
    result_bind[5].buffer_length = sizeof(email);
    result_bind[5].length = &email_length;
    result_bind[5].is_null = &email_is_null;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = status;
    result_bind[6].buffer_length = sizeof(status);
    result_bind[6].length = &status_length;
    result_bind[6].is_null = &status_is_null;

    result_bind[7].buffer_type = MYSQL_TYPE_STRING;
    result_bind[7].buffer = last_login_time;
    result_bind[7].buffer_length = sizeof(last_login_time);
    result_bind[7].length = &last_login_time_length;
    result_bind[7].is_null = &last_login_time_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        User user;
        user.set_userid(userId);
        user.set_article_count(article_count);

        if (!username_is_null) {
            user.set_username(string(username, username_length));
        }
        if (!avatar_is_null) {
            user.set_avatar(string(avatar, avatar_length));
        }
        if (!register_time_is_null) {
            user.set_register_time(string(register_time, register_time_length));
        }
        if (!email_is_null) {
            user.set_email(string(email, email_length));
        }
        if (!status_is_null) {
            user.set_status(string(status, status_length));
        }
        if (!last_login_time_is_null){
            user.set_last_login_time(string(last_login_time, last_login_time_length));
        }

        users.push_back(user);
    }

    mysql_stmt_close(stmt);
    return users;
}


// 分页+排序+状态
// 分页+排序+状态+最近登录时间
vector<User> sql_blog_tool::get_users_by_page_and_sort_and_status(int page, int size, const string &sortField, const string &sortOrder, const string &status) 
{
    vector<User> users;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造 SQL 语句，加入 last_login_time
    string query = "SELECT userId, username, avatar, article_count, register_time, email, status, last_login_time "
                   "FROM user WHERE status = ? ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return users;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[3] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)status.c_str();
    bind[0].buffer_length = status.length();

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &size;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 结果绑定
    int userId, article_count;
    char username[128] = {};
    char avatar[256] = {};
    char register_time[64] = {};
    char email[128] = {};
    char status_res[16] = {};
    char last_login_time[64] = {};  // 新增 last_login_time
    unsigned long username_length, avatar_length, register_time_length, email_length, status_length, last_login_length;
    my_bool username_is_null, avatar_is_null, register_time_is_null, email_is_null, status_is_null, last_login_is_null;

    MYSQL_BIND result_bind[8] = {};  // 增加到 8 个字段
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &userId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = username;
    result_bind[1].buffer_length = sizeof(username);
    result_bind[1].length = &username_length;
    result_bind[1].is_null = &username_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = avatar;
    result_bind[2].buffer_length = sizeof(avatar);
    result_bind[2].length = &avatar_length;
    result_bind[2].is_null = &avatar_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &article_count;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = register_time;
    result_bind[4].buffer_length = sizeof(register_time);
    result_bind[4].length = &register_time_length;
    result_bind[4].is_null = &register_time_is_null;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = email;
    result_bind[5].buffer_length = sizeof(email);
    result_bind[5].length = &email_length;
    result_bind[5].is_null = &email_is_null;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = status_res;
    result_bind[6].buffer_length = sizeof(status_res);
    result_bind[6].length = &status_length;
    result_bind[6].is_null = &status_is_null;

    result_bind[7].buffer_type = MYSQL_TYPE_STRING;
    result_bind[7].buffer = last_login_time;
    result_bind[7].buffer_length = sizeof(last_login_time);
    result_bind[7].length = &last_login_length;
    result_bind[7].is_null = &last_login_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        User user;
        user.set_userid(userId);
        user.set_article_count(article_count);

        if (!username_is_null) {
            user.set_username(string(username, username_length));
        }
        if (!avatar_is_null) {
            user.set_avatar(string(avatar, avatar_length));
        }
        if (!register_time_is_null) {
            user.set_register_time(string(register_time, register_time_length));
        }
        if (!email_is_null) {
            user.set_email(string(email, email_length));
        }
        if (!status_is_null) {
            user.set_status(string(status_res, status_length));
        }
        if (!last_login_is_null) {
            user.set_last_login_time(string(last_login_time, last_login_length));  // 设置 last_login_time
        }

        users.push_back(user);
    }

    mysql_stmt_close(stmt);
    return users;
}


vector<User> sql_blog_tool::get_users_by_page_and_sort_and_search(int page, int size, const string &sortField, const string &sortOrder, const string &search)
{
    vector<User> users;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + search + "%";

    // 构造安全的 SQL 语句
    string query = 
    "SELECT userId, username, avatar, article_count, register_time, email, status, last_login_time "
    "FROM user "
    "WHERE username LIKE ? OR COALESCE(email, '') LIKE ? "
    "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return users;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[4] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &size;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 结果绑定
    int userId, article_count;
    char username[128] = {};
    char avatar[256] = {};
    char register_time[64] = {};
    char email[128] = {};
    char status_res[16] = {};
    char last_login_time[64] = {};
    unsigned long username_length, avatar_length, register_time_length, email_length, status_length, last_login_time_length;
    my_bool username_is_null, avatar_is_null, register_time_is_null, email_is_null, status_is_null, last_login_time_is_null;

    MYSQL_BIND result_bind[8] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &userId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = username;
    result_bind[1].buffer_length = sizeof(username);
    result_bind[1].length = &username_length;
    result_bind[1].is_null = &username_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = avatar;
    result_bind[2].buffer_length = sizeof(avatar);
    result_bind[2].length = &avatar_length;
    result_bind[2].is_null = &avatar_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &article_count;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = register_time;
    result_bind[4].buffer_length = sizeof(register_time);
    result_bind[4].length = &register_time_length;
    result_bind[4].is_null = &register_time_is_null;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = email;
    result_bind[5].buffer_length = sizeof(email);
    result_bind[5].length = &email_length;
    result_bind[5].is_null = &email_is_null;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = status_res;
    result_bind[6].buffer_length = sizeof(status_res);
    result_bind[6].length = &status_length;
    result_bind[6].is_null = &status_is_null;

    result_bind[7].buffer_type = MYSQL_TYPE_STRING;
    result_bind[7].buffer = last_login_time;
    result_bind[7].buffer_length = sizeof(last_login_time);
    result_bind[7].length = &last_login_time_length;
    result_bind[7].is_null = &last_login_time_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        User user;
        user.set_userid(userId);
        user.set_article_count(article_count);

        if (!username_is_null) {
            user.set_username(string(username, username_length));
        }
        if (!avatar_is_null) {
            user.set_avatar(string(avatar, avatar_length));
        }
        if (!register_time_is_null) {
            user.set_register_time(string(register_time, register_time_length));
        }
        if (!email_is_null) {
            user.set_email(string(email, email_length));
        }
        if (!status_is_null) {
            user.set_status(string(status_res, status_length));
        }
        if (!last_login_time_is_null) {
            user.set_last_login_time(string(last_login_time, last_login_time_length));
        }

        users.push_back(user);
    }

    mysql_stmt_close(stmt);
    return users;
}


vector<User> sql_blog_tool::get_users_by_page_and_sort_and_status_search(int page, int size, const string &sortField, const string &sortOrder, const string &status, const string &search)
{
    vector<User> users;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    string searchPattern = "%" + search + "%";

    string query = "SELECT userId, username, avatar, article_count, register_time, last_login_time, email, status "
                   "FROM user WHERE (username LIKE ? OR COALESCE(email, '') LIKE ?) AND status = ? "
                   "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return users;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    int offset = (page - 1) * size;

    MYSQL_BIND bind[5] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)status.c_str();
    bind[2].buffer_length = status.length();

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &size;

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 结果绑定
    int userId, article_count;
    char username[128] = {};
    char avatar[256] = {};
    char register_time[64] = {};
    char last_login_time[64] = {};  // 新增字段
    char email[128] = {};
    char status_res[16] = {};
    unsigned long username_length, avatar_length, register_time_length, last_login_time_length, email_length, status_length;
    my_bool username_is_null, avatar_is_null, register_time_is_null, last_login_time_is_null, email_is_null, status_is_null;

    MYSQL_BIND result_bind[8] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &userId;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = username;
    result_bind[1].buffer_length = sizeof(username);
    result_bind[1].length = &username_length;
    result_bind[1].is_null = &username_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = avatar;
    result_bind[2].buffer_length = sizeof(avatar);
    result_bind[2].length = &avatar_length;
    result_bind[2].is_null = &avatar_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_LONG;
    result_bind[3].buffer = &article_count;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = register_time;
    result_bind[4].buffer_length = sizeof(register_time);
    result_bind[4].length = &register_time_length;
    result_bind[4].is_null = &register_time_is_null;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = last_login_time;  // 绑定 last_login_time
    result_bind[5].buffer_length = sizeof(last_login_time);
    result_bind[5].length = &last_login_time_length;
    result_bind[5].is_null = &last_login_time_is_null;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = email;
    result_bind[6].buffer_length = sizeof(email);
    result_bind[6].length = &email_length;
    result_bind[6].is_null = &email_is_null;

    result_bind[7].buffer_type = MYSQL_TYPE_STRING;
    result_bind[7].buffer = status_res;
    result_bind[7].buffer_length = sizeof(status_res);
    result_bind[7].length = &status_length;
    result_bind[7].is_null = &status_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return users;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        User user;
        user.set_userid(userId);
        user.set_article_count(article_count);

        if (!username_is_null) {
            user.set_username(string(username, username_length));
        }
        if (!avatar_is_null) {
            user.set_avatar(string(avatar, avatar_length));
        }
        if (!register_time_is_null) {
            user.set_register_time(string(register_time, register_time_length));
        }
        if (!last_login_time_is_null) {  // 设置 last_login_time
            user.set_last_login_time(string(last_login_time, last_login_time_length));
        }
        if (!email_is_null) {
            user.set_email(string(email, email_length));
        }
        if (!status_is_null) {
            user.set_status(string(status_res, status_length));
        }

        users.push_back(user);
    }

    mysql_stmt_close(stmt);
    return users;
}

// 获取所有分类
vector<Categories> sql_blog_tool::get_categories()
{
    vector<Categories> categories;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    string query = "SELECT id, name, description, created_at, updated_at FROM categories";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categories;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length; 
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length; 
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;  
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        Categories categorie;
        categorie.set_id(id);   
        if(!name_is_null){
            categorie.set_name(string(name, name_length));
        }
        if(!description_is_null){
            categorie.set_description(string(description, description_length));
        }
        if(!created_at_is_null){
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if(!updated_at_is_null){
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }   
        categories.push_back(categorie);    
    }

    mysql_stmt_close(stmt);
    return categories;
}

vector<Categories> sql_blog_tool::get_categories_by_page_and_sort(int page, int size, const string &sortField, const string &sortOrder)
{
    vector<Categories> categories;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造安全的 SQL 语句（字段名不能用 ? 绑定参数，因此只能拼接）
    string query = "SELECT id, name, description, created_at, updated_at FROM categories ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categories;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &size;
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Categories categorie;
        categorie.set_id(id);

        if(!name_is_null){
            categorie.set_name(string(name, name_length));
        }
        if(!description_is_null){
            categorie.set_description(string(description, description_length));
        }
        if(!created_at_is_null){
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if(!updated_at_is_null){
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }

        categories.push_back(categorie);
    }

    mysql_stmt_close(stmt);
    return categories;
}

vector<Categories> sql_blog_tool::get_categories_by_page_and_sort_and_search(int page, int size, const string &sortField, const string &sortOrder, const string &search)
{
    vector<Categories> categories;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + search + "%";

    // 构造安全的 SQL 语句
    string query = 
    "SELECT id, name, description, created_at, updated_at "
    "FROM categories "
    "WHERE name LIKE ? OR COALESCE(description, '') LIKE ? "
    "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categories;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[4] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &size;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Categories categorie;
        categorie.set_id(id);
        
        if (!name_is_null) {
            categorie.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            categorie.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }

        categories.push_back(categorie);
    }

    mysql_stmt_close(stmt);
    return categories;
}

vector<Categories> sql_blog_tool::get_categories_by_articles_count(int page, int size, const string &sortOrder)
{
    vector<Categories> categories;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 查询所有分类
    string query = "SELECT id, name, description, created_at, updated_at FROM categories";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categories;
    }

    // 执行查询
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) ||
        mysql_stmt_execute(stmt)) {
        cerr << "Failed to execute query: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    vector<pair<Categories, int>> categories_with_count;
    
    // 取出所有分类并获取其博客数量
    while (mysql_stmt_fetch(stmt) == 0) {
        Categories categorie;
        categorie.set_id(id);

        if(!name_is_null){
            categorie.set_name(string(name, name_length));
        }
        if(!description_is_null){
            categorie.set_description(string(description, description_length));
        }
        if(!created_at_is_null){
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if(!updated_at_is_null){
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }

        // 获取每个分类的博客数量
        int articlesCount = get_total_blog_count_by_category(id);
        categories_with_count.push_back({categorie, articlesCount});
    }

    mysql_stmt_close(stmt);
    
    // 根据博客数量排序
    if (sortOrder == "asc") {
        sort(categories_with_count.begin(), categories_with_count.end(), 
            [](const pair<Categories, int>& a, const pair<Categories, int>& b) {
                return a.second < b.second;
            });
    } else {
        sort(categories_with_count.begin(), categories_with_count.end(), 
            [](const pair<Categories, int>& a, const pair<Categories, int>& b) {
                return a.second > b.second;
            });
    }
    
    // 分页处理
    int start = (page - 1) * size;
    int end = min(start + size, (int)categories_with_count.size());
    
    for (int i = start; i < end; i++) {
        categories.push_back(categories_with_count[i].first);
    }
    
    return categories;
}

vector<Categories> sql_blog_tool::get_categories_by_articles_count_and_search(int page, int size, const string &sortOrder, const string &searchKeyword)
{
    vector<Categories> categories;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 根据关键词搜索分类
    string query = "SELECT id, name, description, created_at, updated_at FROM categories WHERE "
                   "name LIKE ? OR description LIKE ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categories;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 准备搜索参数
    string search_param = "%" + searchKeyword + "%";
    
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)search_param.c_str();
    bind[0].buffer_length = search_param.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)search_param.c_str();
    bind[1].buffer_length = search_param.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 执行SQL语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categories;
    }

    vector<pair<Categories, int>> categories_with_count;
    
    // 取出所有符合搜索条件的分类并获取其博客数量
    while (mysql_stmt_fetch(stmt) == 0) {
        Categories categorie;
        categorie.set_id(id);

        if(!name_is_null){
            categorie.set_name(string(name, name_length));
        }
        if(!description_is_null){
            categorie.set_description(string(description, description_length));
        }
        if(!created_at_is_null){
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if(!updated_at_is_null){
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }

        // 获取每个分类的博客数量
        int articlesCount = get_total_blog_count_by_category(id);
        categories_with_count.push_back({categorie, articlesCount});
    }

    mysql_stmt_close(stmt);
    
    // 根据博客数量排序
    if (sortOrder == "asc") {
        sort(categories_with_count.begin(), categories_with_count.end(), 
            [](const pair<Categories, int>& a, const pair<Categories, int>& b) {
                return a.second < b.second;
            });
    } else {
        sort(categories_with_count.begin(), categories_with_count.end(), 
            [](const pair<Categories, int>& a, const pair<Categories, int>& b) {
                return a.second > b.second;
            });
    }
    
    // 分页处理
    int start = (page - 1) * size;
    int end = min(start + size, (int)categories_with_count.size());
    
    for (int i = start; i < end && i < categories_with_count.size(); i++) {
        categories.push_back(categories_with_count[i].first);
    }
    
    return categories;
}

// 获取所有标签
vector<string> sql_blog_tool::get_all_tags()
{
    vector<string> tags;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 直接使用普通查询而不是预处理语句，更适合简单查询
    if (mysql_query(mysql, "SELECT name FROM tags")) {
        cerr << "mysql_query() failed: " << mysql_error(mysql) << endl;
        return tags;
    }

    MYSQL_RES* result = mysql_store_result(mysql);
    if (!result) {
        cerr << "mysql_store_result() failed: " << mysql_error(mysql) << endl;
        return tags;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        if (row[0]) {
            tags.push_back(string(row[0], lengths[0]));
        }
    }

    mysql_free_result(result);
    return tags;
}

// 获取标签按博客数量排序
// 获取标签按博客数量排序
vector<Tags> sql_blog_tool::get_tags_by_blog_count(int page, int size, const string &sortOrder)
{
    vector<Tags> tags;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 查询所有标签
    string query = "SELECT id, name, description, created_at, updated_at FROM tags";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tags;
    }

    // 执行查询 - 合并prepare和execute步骤的错误检查
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()) ||
        mysql_stmt_execute(stmt)) {
        cerr << "Failed to execute query: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;    
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null; 

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING; 
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING; 
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }       

    vector<pair<Tags, int>> tags_with_count;
    
    // 取出所有标签并获取其博客数量
    while (mysql_stmt_fetch(stmt) == 0) {
        Tags tag;
        tag.set_id(id);

        if (!name_is_null) {
            tag.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            tag.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            tag.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            tag.set_updated_at(string(updated_at, updated_at_length));
        }

        // 获取每个标签的博客数量
        int articlesCount = get_total_blog_count_by_tag(id);
        tags_with_count.push_back({tag, articlesCount});
    }
    
    mysql_stmt_close(stmt);

    // 根据博客数量排序
    if (sortOrder == "asc") {
        sort(tags_with_count.begin(), tags_with_count.end(), 
            [](const pair<Tags, int>& a, const pair<Tags, int>& b) {
                return a.second < b.second; 
            });
    } else {
        sort(tags_with_count.begin(), tags_with_count.end(), 
            [](const pair<Tags, int>& a, const pair<Tags, int>& b) {
                return a.second > b.second;
            }); 
    }

    // 分页处理
    int start = (page - 1) * size;
    int end = min(start + size, (int)tags_with_count.size());   
    
    for (int i = start; i < end; i++) {
        tags.push_back(tags_with_count[i].first);
    }

    return tags;
}

// 获取标签按博客数量排序+搜索
vector<Tags> sql_blog_tool::get_tags_by_blog_count_and_search(int page, int size, const string &sortOrder, const string &searchKeyword)
{
    vector<Tags> tags;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // Set character encoding
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // Properly format search parameter with wildcards
    string search_param = "%" + searchKeyword + "%";

    // Prepare SQL query
    string query = "SELECT id, name, description, created_at, updated_at FROM tags WHERE "
                   "name LIKE ? OR description LIKE ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tags;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // Bind search parameters
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;        
    bind[0].buffer = (void*)search_param.c_str();
    bind[0].buffer_length = search_param.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)search_param.c_str();
    bind[1].buffer_length = search_param.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // Prepare result binding variables
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING; 
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING; 
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;  
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;   
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }   

    vector<pair<Tags, int>> tags_with_count;
    
    while (mysql_stmt_fetch(stmt) == 0) {
        Tags tag;   
        tag.set_id(id);

        if (!name_is_null) {
            tag.set_name(string(name, name_length));
        }   
        if (!description_is_null) {
            tag.set_description(string(description, description_length));
        }   
        if (!created_at_is_null) {
            tag.set_created_at(string(created_at, created_at_length));
        }   
        if (!updated_at_is_null) {
            tag.set_updated_at(string(updated_at, updated_at_length));
        }   

        // Get blog count for each tag
        int articlesCount = get_total_blog_count_by_tag(id);
        tags_with_count.push_back({tag, articlesCount});
    }   

    mysql_stmt_close(stmt);

    // Sort by blog count
    if (sortOrder == "asc") {
        sort(tags_with_count.begin(), tags_with_count.end(), 
            [](const pair<Tags, int>& a, const pair<Tags, int>& b) {
                return a.second < b.second;
            });
    } else {
        sort(tags_with_count.begin(), tags_with_count.end(), 
            [](const pair<Tags, int>& a, const pair<Tags, int>& b) {
                return a.second > b.second;
            });
    }

    // Pagination
    int start = (page - 1) * size;
    int end = min(start + size, (int)tags_with_count.size());
    
    // Only process necessary elements for current page
    tags.reserve(end - start);  // Pre-allocate memory for better performance
    for (int i = start; i < end; i++) {
        tags.push_back(tags_with_count[i].first);
    }

    return tags;
}

// 获取标签按指定字段排序
vector<Tags> sql_blog_tool::get_tags_by_page_and_sort(int page, int size, const string &sortField, const string &sortOrder)
{
    vector<Tags> tags;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造安全的 SQL 语句（字段名不能用 ? 绑定参数，因此只能拼接）
    string query = "SELECT id, name, description, created_at, updated_at FROM tags ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tags;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &size;
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Tags tag;
        tag.set_id(id);

        if (!name_is_null) {
            tag.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            tag.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            tag.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            tag.set_updated_at(string(updated_at, updated_at_length));
        }

        tags.push_back(tag);
    }

    mysql_stmt_close(stmt);
    return tags;
}

// 获取标签按指定字段排序+搜索
vector<Tags> sql_blog_tool::get_tags_by_page_and_sort_and_search(int page, int size, const string &sortField, const string &sortOrder, const string &search)
{
    vector<Tags> tags;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + search + "%";

    // 构造安全的 SQL 语句
    string query = 
    "SELECT id, name, description, created_at, updated_at "
    "FROM tags "
    "WHERE name LIKE ? OR COALESCE(description, '') LIKE ? "
    "ORDER BY " + sortField + " " + sortOrder + " LIMIT ? OFFSET ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tags;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    int offset = (page - 1) * size;

    // 绑定参数
    MYSQL_BIND bind[4] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &size;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &offset;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 结果绑定
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 取出数据
    while (mysql_stmt_fetch(stmt) == 0) {
        Tags tag;
        tag.set_id(id);
        
        if (!name_is_null) {
            tag.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            tag.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            tag.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            tag.set_updated_at(string(updated_at, updated_at_length));
        }

        tags.push_back(tag);
    }

    mysql_stmt_close(stmt);
    return tags;
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

// 根据分类获取博客的总条数
int sql_blog_tool::get_total_blog_count_by_category(int categoryId)
{
    int count = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句，根据分类ID过滤
    const char* query = "SELECT COUNT(*) FROM blog WHERE category_id = ?";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return 0;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&categoryId;

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&count;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        std::cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt) != 0) {
        std::cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 清理
    mysql_stmt_close(stmt);

    return count;
}

// 获取符合搜索条件的博客总数
int sql_blog_tool::get_total_blog_count_by_search(const string& keyword) {
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    const char* query = 
        "SELECT COUNT(*) FROM blog WHERE title LIKE ? OR content LIKE ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        // 可添加日志输出
        return 0;
    }

    // 准备预处理语句
    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 构造模糊匹配字符串
    string searchPattern = "%" + keyword + "%";
    MYSQL_BIND bind_params[2];
    memset(bind_params, 0, sizeof(bind_params));

    // 绑定两个参数（LIKE ?）
    for (int i = 0; i < 2; ++i) {
        bind_params[i].buffer_type = MYSQL_TYPE_STRING;
        bind_params[i].buffer = (char*)searchPattern.c_str();
        bind_params[i].buffer_length = searchPattern.length();
    }

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind_params)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 定义结果绑定结构体
    long long count = 0;  // COUNT(*) 返回 long long 类型
    MYSQL_BIND bind_result;
    memset(&bind_result, 0, sizeof(bind_result));
    bind_result.buffer_type = MYSQL_TYPE_LONGLONG;
    bind_result.buffer = &count;

    // 绑定结果
    if (mysql_stmt_bind_result(stmt, &bind_result)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    mysql_stmt_close(stmt);
    return static_cast<int>(count);  // 转换为 int 返回
}

// 获取符合搜索条件的评论总数
int sql_blog_tool::get_total_comments_count_by_search(const string &keyword)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    const char* query = 
        "SELECT COUNT(*) FROM blog WHERE username LIKE ? OR content LIKE ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        // 可添加日志输出
        return 0;
    }

    // 准备预处理语句
    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 构造模糊匹配字符串
    string searchPattern = "%" + keyword + "%";
    MYSQL_BIND bind_params[2];
    memset(bind_params, 0, sizeof(bind_params));

    // 绑定两个参数（LIKE ?）
    for (int i = 0; i < 2; ++i) {
        bind_params[i].buffer_type = MYSQL_TYPE_STRING;
        bind_params[i].buffer = (char*)searchPattern.c_str();
        bind_params[i].buffer_length = searchPattern.length();
    }

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind_params)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 定义结果绑定结构体
    long long count = 0;  // COUNT(*) 返回 long long 类型
    MYSQL_BIND bind_result;
    memset(&bind_result, 0, sizeof(bind_result));
    bind_result.buffer_type = MYSQL_TYPE_LONGLONG;
    bind_result.buffer = &count;

    // 绑定结果
    if (mysql_stmt_bind_result(stmt, &bind_result)) {
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return 0;
    }

    mysql_stmt_close(stmt);
    return static_cast<int>(count);  // 转换为 int 返回
}

// 获取符合搜索条件的分类总数
int sql_blog_tool::get_total_categories_count_by_search(const string &keyword)
{
    int count = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    // 构造带通配符的搜索模式
    string searchPattern = "%" + keyword + "%";
    
    // 构造安全的 SQL 语句
    string query = "SELECT COUNT(*) FROM categories WHERE name LIKE ? OR COALESCE(description, '') LIKE ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return count;
    }
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 绑定参数
    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 结果绑定
    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 取出数据
    if (mysql_stmt_fetch(stmt) != 0) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    mysql_stmt_close(stmt);
    return count;
}

int sql_blog_tool::get_total_blog_count_by_category_and_search(int categoryId, const string& keyword)
{
    int totalCount = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    string query = "SELECT COUNT(*) FROM blog WHERE category_id = ? AND (title LIKE ? OR content LIKE ?)";
    const char* query_cstr = query.c_str();

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return totalCount;
    }

    if (mysql_stmt_prepare(stmt, query_cstr, strlen(query_cstr))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return totalCount;
    }

    string searchPattern = "%" + keyword + "%";

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&categoryId;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)searchPattern.c_str();
    bind[2].buffer_length = searchPattern.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return totalCount;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return totalCount;
    }

    MYSQL_BIND result_bind;
    memset(&result_bind, 0, sizeof(result_bind));

    result_bind.buffer_type = MYSQL_TYPE_LONG;
    result_bind.buffer = (char*)&totalCount;

    if (mysql_stmt_bind_result(stmt, &result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return totalCount;
    }

    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
    }

    mysql_stmt_close(stmt);
    return totalCount;
}

// 获取用户的总数
int sql_blog_tool::get_user_count()
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句
    const char* query = "SELECT COUNT(*) FROM user;";

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

// 根据用户状态获取用户总数
int sql_blog_tool::get_user_count_by_status(const string& status)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池获取连接
    connectionRAII conn(&mysql, connpool);

    // SQL 查询
    const char* query = "SELECT COUNT(*) FROM user WHERE status = ?;";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return 0;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定输入参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));

    bind_param.buffer_type = MYSQL_TYPE_STRING;
    bind_param.buffer = (char*)status.c_str();
    bind_param.buffer_length = status.length();

    if (mysql_stmt_bind_param(stmt, &bind_param)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定结果
    int count = 0;
    MYSQL_BIND bind_result;
    memset(&bind_result, 0, sizeof(bind_result));

    bind_result.buffer_type = MYSQL_TYPE_LONG;
    bind_result.buffer = &count;
    bind_result.buffer_length = sizeof(count);

    if (mysql_stmt_bind_result(stmt, &bind_result)) {
        std::cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 取出结果
    if (mysql_stmt_fetch(stmt)) {
        std::cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 关闭 stmt
    mysql_stmt_close(stmt);
    return count;
}

// 根据搜索获取用户总数
int sql_blog_tool::get_user_count_by_search(string search)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句，根据分类ID过滤
    const char* query = "SELECT COUNT(*) FROM user WHERE username LIKE ? OR email LIKE ?;";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return 0;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定参数
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)search.c_str();
    bind[0].buffer_length = search.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)search.c_str();
    bind[1].buffer_length = search.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取结果
    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        std::cerr << "mysql_stmt_result_metadata() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 读取查询结果
    MYSQL_ROW row = mysql_fetch_row(prepare_meta_result);
    int count = 0;
    if (row && row[0]) {
        count = std::atoi(row[0]); // 将字符串转为整数
    }

    // 清理
    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);

    return count;
}

// 根据状态和搜索关键词获取用户总数
int sql_blog_tool::get_user_count_by_status_search(string status, string search)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + search + "%";

    // 构造 SQL 查询语句，使用 COALESCE 处理可能的 NULL 邮箱
    const char* query = "SELECT COUNT(*) FROM user WHERE (username LIKE ? OR COALESCE(email, '') LIKE ?) AND status = ?;";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return 0;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    // 使用 searchPattern 进行模糊搜索
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)status.c_str();
    bind[2].buffer_length = status.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定查询结果
    int count = 0;
    MYSQL_BIND result_bind;
    memset(&result_bind, 0, sizeof(result_bind));

    result_bind.buffer_type = MYSQL_TYPE_LONG;
    result_bind.buffer = &count;
    result_bind.is_null = nullptr;
    result_bind.length = nullptr;

    if (mysql_stmt_bind_result(stmt, &result_bind)) {
        std::cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取结果
    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result == 1) {
        std::cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 清理
    mysql_stmt_close(stmt);

    return count;
}

// 获取分类总数
// 获取分类总数
int sql_blog_tool::get_categorie_count()
{
    int count = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    // 构造安全的 SQL 语句
    string query = "SELECT COUNT(*) FROM categories";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return count;
    }
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 结果绑定
    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 取出数据
    if (mysql_stmt_fetch(stmt) != 0) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    mysql_stmt_close(stmt);
    return count;
}

// 获取评论总数
int sql_blog_tool::get_comment_count()
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句
    const char* query = "SELECT COUNT(*) FROM blog_comments;";

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

// 获取点赞总数
int sql_blog_tool::get_like_count()
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 查询语句
    const char* query = "SELECT COUNT(*) FROM blog_likes;";

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

// 获取标签总数
// 获取标签总数
int sql_blog_tool::get_tag_count()
{
    int count = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    // 构造安全的 SQL 语句
    string query = "SELECT COUNT(*) FROM tags";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return count;
    }
    
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 结果绑定
    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    // 取出数据
    if (mysql_stmt_fetch(stmt) != 0) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return count;
    }
    
    mysql_stmt_close(stmt);
    return count;
}

// 获取符合搜索条件的标签总数
// 获取符合搜索条件的标签总数
int sql_blog_tool::get_total_tags_count_by_search(const string& search)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构造带通配符的搜索模式
    string searchPattern = "%" + search + "%";

    // 构造 SQL 查询语句，使用 COALESCE 处理可能的 NULL 描述
    const char* query = "SELECT COUNT(*) FROM tags WHERE name LIKE ? OR COALESCE(description, '') LIKE ?";
    
    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return 0;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定参数
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    // 使用 searchPattern 进行模糊搜索
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)searchPattern.c_str();
    bind[0].buffer_length = searchPattern.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)searchPattern.c_str();
    bind[1].buffer_length = searchPattern.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }   

    // 绑定查询结果
    int count = 0;
    MYSQL_BIND result_bind;
    memset(&result_bind, 0, sizeof(result_bind));
    
    result_bind.buffer_type = MYSQL_TYPE_LONG;
    result_bind.buffer = &count;
    result_bind.is_null = nullptr;
    result_bind.length = nullptr;
    
    if (mysql_stmt_bind_result(stmt, &result_bind)) {
        std::cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }       

    // 获取结果
    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result == 1) {
        std::cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    // 清理
    mysql_stmt_close(stmt);
    return count;
}

// 获取指定标签的博客总数
int sql_blog_tool::get_total_blog_count_by_tag(int tagid) {
    int count = 0;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);
    
    // 检查连接是否成功
    if (!mysql) {
        cerr << "Failed to get MySQL connection" << endl;
        return 0;
    }
    
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    // 准备SQL语句
    string query = "SELECT COUNT(*) FROM blog_tags WHERE tag_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return 0;
    }
    
    // 准备预处理语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&tagid;
    bind[0].is_null = 0;
    bind[0].length = 0;
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 绑定结果
    MYSQL_BIND result_bind[1] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;
    
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 获取结果
    if (mysql_stmt_store_result(stmt)) {
        cerr << "mysql_stmt_store_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result != 0 && fetch_result != MYSQL_NO_DATA) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }
    
    // 清理资源
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    
    return count;
}

// 通过博客id查询博客内容 
Blog sql_blog_tool::select_blog_by_id(int blogid)
{
	connection_pool* connpool = connection_pool::GetInstance();
	MYSQL* mysql = nullptr;
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 准备SQL语句
	const char* query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count, thumbnail FROM blog WHERE blogId = ?";
	MYSQL_STMT* stmt = mysql_stmt_init(mysql);
	if (!stmt) {
		cerr << "mysql_stmt_init() failed" << endl;
		return Blog();
	}

	if (mysql_stmt_prepare(stmt, query, strlen(query))) {
		cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return Blog();
	}

	// 绑定参数
	MYSQL_BIND bind_param[1];
	memset(bind_param, 0, sizeof(bind_param));
	bind_param[0].buffer_type = MYSQL_TYPE_LONG;
	bind_param[0].buffer = (char*)&blogid;

	if (mysql_stmt_bind_param(stmt, bind_param)) {
		cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return Blog();
	}

	// 执行查询
	if (mysql_stmt_execute(stmt)) {
		cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return Blog();
	}

	// 准备结果绑定
	MYSQL_BIND bind_result[9];
	memset(bind_result, 0, sizeof(bind_result));

	int blog_id;
	char title[256];
	char content[65535];
	int user_id;
	char post_time[20];
	int category_id;
	char updated_at[20];
	int view_count;
    char thumbnail[256];
	unsigned long title_length, content_length, post_time_length, updated_at_length, thumbnail_length;
	my_bool thumbnail_is_null;

	bind_result[0].buffer_type = MYSQL_TYPE_LONG;
	bind_result[0].buffer = (char*)&blog_id;

	bind_result[1].buffer_type = MYSQL_TYPE_STRING;
	bind_result[1].buffer = title;
	bind_result[1].buffer_length = sizeof(title);
	bind_result[1].length = &title_length;

	bind_result[2].buffer_type = MYSQL_TYPE_STRING;
	bind_result[2].buffer = content;
	bind_result[2].buffer_length = sizeof(content);
	bind_result[2].length = &content_length;

	bind_result[3].buffer_type = MYSQL_TYPE_LONG;
	bind_result[3].buffer = (char*)&user_id;

	bind_result[4].buffer_type = MYSQL_TYPE_STRING;
	bind_result[4].buffer = post_time;
	bind_result[4].buffer_length = sizeof(post_time);
	bind_result[4].length = &post_time_length;

	bind_result[5].buffer_type = MYSQL_TYPE_LONG;
	bind_result[5].buffer = (char*)&category_id;

	bind_result[6].buffer_type = MYSQL_TYPE_STRING;
	bind_result[6].buffer = updated_at;
	bind_result[6].buffer_length = sizeof(updated_at);
	bind_result[6].length = &updated_at_length;

	bind_result[7].buffer_type = MYSQL_TYPE_LONG;
	bind_result[7].buffer = (char*)&view_count;

	bind_result[8].buffer_type = MYSQL_TYPE_STRING;
	bind_result[8].buffer = thumbnail;
	bind_result[8].buffer_length = sizeof(thumbnail);
	bind_result[8].length = &thumbnail_length;
	bind_result[8].is_null = &thumbnail_is_null;

	if (mysql_stmt_bind_result(stmt, bind_result)) {
		cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
		mysql_stmt_close(stmt);
		return Blog();
	}

	// 获取结果
	if (mysql_stmt_fetch(stmt)) {
		mysql_stmt_close(stmt);
		return Blog();
	}

	// 创建Blog对象并填充数据
	Blog blog;
	blog.set_blog_id(blog_id);
	blog.set_blog_title(string(title, title_length));
	blog.set_blog_content(string(content, content_length));
	blog.set_user_id(user_id);
	blog.set_blog_postTime(string(post_time, post_time_length));
	blog.set_category_id(category_id);
	blog.set_updatedAt(string(updated_at, updated_at_length));
	blog.set_views(view_count);
    if (thumbnail_is_null) {
        blog.set_thumbnail("");
    } else {
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
    }

	mysql_stmt_close(stmt);
	return blog;
}

// 通过博客id获取上一篇博客
Blog sql_blog_tool::get_prev_blog_by_id(int blogid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    // 从连接池中获取 MySQL 连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集为 UTF8
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句 - 获取当前博客的发布时间
    const char* query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count FROM blog WHERE postTime < (SELECT postTime FROM blog WHERE blogId = ?) ORDER BY postTime DESC LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return Blog();
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 绑定参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 准备结果绑定
    MYSQL_BIND bind_result[8];
    memset(bind_result, 0, sizeof(bind_result));
    
    int prev_id;
    char title[200];
    char content[65535];
    int user_id;
    char post_time[20];
    int category_id;
    char updated_at[20];
    int view_count;
    unsigned long title_length, content_length, post_time_length, updated_at_length;

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&prev_id;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = title;
    bind_result[1].buffer_length = sizeof(title);
    bind_result[1].length = &title_length;

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = content;
    bind_result[2].buffer_length = sizeof(content);
    bind_result[2].length = &content_length;

    bind_result[3].buffer_type = MYSQL_TYPE_LONG;
    bind_result[3].buffer = (char*)&user_id;

    bind_result[4].buffer_type = MYSQL_TYPE_STRING;
    bind_result[4].buffer = post_time;
    bind_result[4].buffer_length = sizeof(post_time);
    bind_result[4].length = &post_time_length;

    bind_result[5].buffer_type = MYSQL_TYPE_LONG;
    bind_result[5].buffer = (char*)&category_id;

    bind_result[6].buffer_type = MYSQL_TYPE_STRING;
    bind_result[6].buffer = updated_at;
    bind_result[6].buffer_length = sizeof(updated_at);
    bind_result[6].length = &updated_at_length;

    bind_result[7].buffer_type = MYSQL_TYPE_LONG;
    bind_result[7].buffer = (char*)&view_count;

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 创建Blog对象并填充数据
    Blog blog;
    blog.set_blog_id(prev_id);
    blog.set_blog_title(string(title, title_length));
    blog.set_blog_content(string(content, content_length));
    blog.set_user_id(user_id);
    blog.set_blog_postTime(string(post_time, post_time_length));
    blog.set_category_id(category_id);
    blog.set_updatedAt(string(updated_at, updated_at_length));
    blog.set_views(view_count);
    mysql_stmt_close(stmt);
    return blog;
}

// 通过博客id获取下一篇博客
Blog sql_blog_tool::get_next_blog_by_id(int blogid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集为 UTF8
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句 - 获取当前博客的发布时间
    const char* query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count FROM blog WHERE postTime > (SELECT postTime FROM blog WHERE blogId = ?) ORDER BY postTime ASC LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return Blog();
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 绑定参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 准备结果绑定
    MYSQL_BIND bind_result[8];
    memset(bind_result, 0, sizeof(bind_result));

    int next_id;
    char title[200];
    char content[65535];
    int user_id;
    char post_time[20];
    int category_id;
    char updated_at[20];
    int view_count;
    unsigned long title_length, content_length, post_time_length, updated_at_length;

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&next_id;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = title;
    bind_result[1].buffer_length = sizeof(title);
    bind_result[1].length = &title_length;

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = content;
    bind_result[2].buffer_length = sizeof(content);
    bind_result[2].length = &content_length;

    bind_result[3].buffer_type = MYSQL_TYPE_LONG;
    bind_result[3].buffer = (char*)&user_id;

    bind_result[4].buffer_type = MYSQL_TYPE_STRING;
    bind_result[4].buffer = post_time;
    bind_result[4].buffer_length = sizeof(post_time);
    bind_result[4].length = &post_time_length;

    bind_result[5].buffer_type = MYSQL_TYPE_LONG;
    bind_result[5].buffer = (char*)&category_id;

    bind_result[6].buffer_type = MYSQL_TYPE_STRING;
    bind_result[6].buffer = updated_at;
    bind_result[6].buffer_length = sizeof(updated_at);
    bind_result[6].length = &updated_at_length;

    bind_result[7].buffer_type = MYSQL_TYPE_LONG;
    bind_result[7].buffer = (char*)&view_count;

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        mysql_stmt_close(stmt);
        return Blog();
    }

    // 创建Blog对象并填充数据
    Blog blog;
    blog.set_blog_id(next_id);
    blog.set_blog_title(string(title, title_length));
    blog.set_blog_content(string(content, content_length));
    blog.set_user_id(user_id);
    blog.set_blog_postTime(string(post_time, post_time_length));
    blog.set_category_id(category_id);
    blog.set_updatedAt(string(updated_at, updated_at_length));
    blog.set_views(view_count);     

    mysql_stmt_close(stmt);
    return blog;
}

// 获取相关文章
vector<Blog> sql_blog_tool::get_related_blogs(int category, int excludeId, int size)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集为 UTF8
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理 SQL 查询语句
    const char* query = "SELECT blogId, title, content, userId, postTime, category_id, updatedAt, view_count, thumbnail FROM blog WHERE category_id = ? AND blogId != ? ORDER BY postTime DESC LIMIT ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return vector<Blog>();
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return vector<Blog>();
    }

    MYSQL_BIND bind_param[3];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&category;

    bind_param[1].buffer_type = MYSQL_TYPE_LONG;
    bind_param[1].buffer = (char*)&excludeId;

    bind_param[2].buffer_type = MYSQL_TYPE_LONG;
    bind_param[2].buffer = (char*)&size;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return vector<Blog>();
    }   

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return vector<Blog>();
    }   

    MYSQL_BIND bind_result[9];
    memset(bind_result, 0, sizeof(bind_result));

    int blog_id;
    char title[200];    
    char content[65535];
    int user_id;
    char post_time[20];
    int category_id;
    char updated_at[20];
    int view_count;
    char thumbnail[256];
    unsigned long title_length, content_length, post_time_length, updated_at_length, thumbnail_length;

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&blog_id;

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = title;
    bind_result[1].buffer_length = sizeof(title);
    bind_result[1].length = &title_length;

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = content;    
    bind_result[2].buffer_length = sizeof(content);
    bind_result[2].length = &content_length;

    bind_result[3].buffer_type = MYSQL_TYPE_LONG;
    bind_result[3].buffer = (char*)&user_id;

    bind_result[4].buffer_type = MYSQL_TYPE_STRING;
    bind_result[4].buffer = post_time;
    bind_result[4].buffer_length = sizeof(post_time);
    bind_result[4].length = &post_time_length;

    bind_result[5].buffer_type = MYSQL_TYPE_LONG;
    bind_result[5].buffer = (char*)&category_id;

    bind_result[6].buffer_type = MYSQL_TYPE_STRING;
    bind_result[6].buffer = updated_at;
    bind_result[6].buffer_length = sizeof(updated_at);
    bind_result[6].length = &updated_at_length;

    bind_result[7].buffer_type = MYSQL_TYPE_LONG;
    bind_result[7].buffer = (char*)&view_count;

    bind_result[8].buffer_type = MYSQL_TYPE_STRING;
    bind_result[8].buffer = thumbnail;
    bind_result[8].buffer_length = sizeof(thumbnail);
    bind_result[8].length = &thumbnail_length;

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return vector<Blog>();
    }

    vector<Blog> relatedBlogs;

    while (mysql_stmt_fetch(stmt) == 0) {
        Blog blog;
        blog.set_blog_id(blog_id);
        blog.set_blog_title(string(title, title_length));
        blog.set_blog_content(string(content, content_length));
        blog.set_user_id(user_id);
        blog.set_blog_postTime(string(post_time, post_time_length));
        blog.set_category_id(category_id);
        blog.set_updatedAt(string(updated_at, updated_at_length));
        blog.set_views(view_count);
        blog.set_thumbnail(string(thumbnail, thumbnail_length));
        relatedBlogs.push_back(blog);
    }

    mysql_stmt_close(stmt);
    return relatedBlogs;
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
bool sql_blog_tool::delete_blog_by_blogid(int blogid)
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
        return false;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定查询参数
    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));

    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&blogid;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 语句（删除操作）
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 判断删除是否成功
    bool success = (mysql_stmt_affected_rows(stmt) > 0);

    if (success) {
        cout << "博客 ID " << blogid << " 删除成功!" << endl;
    } else {
        cout << "未找到博客 ID " << blogid << "，删除失败!" << endl;
    }

    // 关闭 SQL 语句
    mysql_stmt_close(stmt);

    return success;
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
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    if (mysql_ping(mysql) != 0) {
        cerr << "MySQL connection lost, reconnecting..." << endl;
        return User();
    }

    const char* query = "SELECT userId, username, password, avatar, article_count, register_time, email, description, status FROM user WHERE userId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return User();
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User();
    }

    MYSQL_BIND bind_param[1];
    memset(bind_param, 0, sizeof(bind_param));
    bind_param[0].buffer_type = MYSQL_TYPE_LONG;
    bind_param[0].buffer = (char*)&userid;
    bind_param[0].is_null = 0;
    bind_param[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User();
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User();
    }

    MYSQL_BIND bind_result[9];
    memset(bind_result, 0, sizeof(bind_result));

    int userId;
    char username[128] = {0}, password[128] = {0}, avatar[256] = {0};
    int article_count;
    char register_time[64] = {0}, email[128] = {0}, description[256] = {0};
    unsigned long str_length[9]; // 修改为9个元素
    char status[16] = {0}; // 初始化为0

    bind_result[0].buffer_type = MYSQL_TYPE_LONG;
    bind_result[0].buffer = (char*)&userId;
    bind_result[0].length = &str_length[0];

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = (char*)username;
    bind_result[1].buffer_length = sizeof(username);
    bind_result[1].length = &str_length[1];

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = (char*)password;
    bind_result[2].buffer_length = sizeof(password);
    bind_result[2].length = &str_length[2];

    bind_result[3].buffer_type = MYSQL_TYPE_STRING;
    bind_result[3].buffer = (char*)avatar;
    bind_result[3].buffer_length = sizeof(avatar);
    bind_result[3].length = &str_length[3];

    bind_result[4].buffer_type = MYSQL_TYPE_LONG;
    bind_result[4].buffer = (char*)&article_count;
    bind_result[4].length = &str_length[4];

    bind_result[5].buffer_type = MYSQL_TYPE_STRING;
    bind_result[5].buffer = (char*)register_time;
    bind_result[5].buffer_length = sizeof(register_time);
    bind_result[5].length = &str_length[5];

    bind_result[6].buffer_type = MYSQL_TYPE_STRING;
    bind_result[6].buffer = (char*)email;
    bind_result[6].buffer_length = sizeof(email);
    bind_result[6].length = &str_length[6];

    bind_result[7].buffer_type = MYSQL_TYPE_STRING;
    bind_result[7].buffer = (char*)description;
    bind_result[7].buffer_length = sizeof(description);
    bind_result[7].length = &str_length[7];

    bind_result[8].buffer_type = MYSQL_TYPE_STRING;
    bind_result[8].buffer = (char*)status;
    bind_result[8].buffer_length = sizeof(status);
    bind_result[8].length = &str_length[8]; // 修正索引

    if (mysql_stmt_bind_result(stmt, bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User();
    }

    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result != 0 && fetch_result != MYSQL_NO_DATA) {
        cerr << "Fetching data failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return User();
    }

    // 如果没有数据，返回空用户
    if (fetch_result == MYSQL_NO_DATA) {
        mysql_stmt_close(stmt);
        return User();
    }

    User user;
    user.set_userid(userId);
    user.set_username(username);
    user.set_password(password);
    user.set_avatar(avatar);
    user.set_article_count(article_count);
    user.set_register_time(register_time);
    user.set_email(email);
    user.set_description(description);
    user.set_status(status);

    mysql_stmt_close(stmt);
    return user;
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

// 通过用户id获取该用户的文章总数
int sql_blog_tool::get_article_count_by_userid(int userid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    const char* query = "SELECT COUNT(*) FROM blog WHERE userId = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return 0;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&userid;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    int article_count = 0;
    
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&article_count;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return 0;
    }

    mysql_stmt_close(stmt);
    return article_count;
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
bool sql_blog_tool::insert_blog(Blog blog)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从数据库连接池中取出一个连接
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句
    const char* query = "INSERT INTO blog (title, content, userId, postTime) VALUES (?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);

    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 获取Blog对象的数据
    string title = blog.get_blog_title();
    string content = blog.get_blog_content();
    int userId = blog.get_user_id();
    string postTime = blog.get_blog_postTime();

    // 绑定参数
    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

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

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 关闭语句
    mysql_stmt_close(stmt);

    return true;
}

// 写入博客
int sql_blog_tool::add_blog(string title, string content, int userid, int categoryid, string thumbnail_path)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    // 准备SQL语句
    const char* query = "INSERT INTO blog (title, content, userId, category_id, postTime, thumbnail) VALUES (?, ?, ?, ?, NOW(), ?)";
    
    // 初始化预处理语句
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }
    
    // 准备预处理语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }
    
    // 绑定参数
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));
    
    // 绑定标题
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)title.c_str();
    bind[0].buffer_length = title.length();
    
    // 绑定内容
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)content.c_str();
    bind[1].buffer_length = content.length();
    
    // 绑定用户ID
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = (char*)&userid;
    
    // 绑定分类ID
    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = (char*)&categoryid;
    
    // 绑定缩略图路径
    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = (char*)thumbnail_path.c_str();
    bind[4].buffer_length = thumbnail_path.length();
    
    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }
    
    // 执行语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }
    
    // 获取插入的ID
    int blog_id = mysql_stmt_insert_id(stmt);
    
    // 关闭语句
    mysql_stmt_close(stmt);
    
    return blog_id;
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

// 检查该用户名是否已经被注册
bool sql_blog_tool::check_username_is_exist(string username)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建 SQL 查询语句
    string query = "SELECT COUNT(*) FROM user WHERE username = ?";
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

    // 绑定参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));

    bind_param.buffer_type = MYSQL_TYPE_STRING;
    bind_param.buffer = (char*)username.c_str();
    bind_param.buffer_length = username.length();
    bind_param.is_null = 0;

    if (mysql_stmt_bind_param(stmt, &bind_param)) {
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

    // 绑定结果
    MYSQL_BIND bind_result;
    memset(&bind_result, 0, sizeof(bind_result));

    int count = 0;
    bind_result.buffer_type = MYSQL_TYPE_LONG;
    bind_result.buffer = &count;
    bind_result.is_null = 0;

    if (mysql_stmt_bind_result(stmt, &bind_result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return count > 0;
}

// 存储用户头像路径
bool sql_blog_tool::update_avatar_path(string username, string file_path) {
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建 SQL 查询语句
    string query = "UPDATE user SET avatar = ? WHERE username = ?";
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

    // 绑定 file_path 参数
    bind_param[0].buffer_type = MYSQL_TYPE_STRING;
    bind_param[0].buffer = (char*)file_path.c_str();
    bind_param[0].buffer_length = file_path.length();
    bind_param[0].is_null = 0;

    // 绑定 username 参数
    bind_param[1].buffer_type = MYSQL_TYPE_STRING;
    bind_param[1].buffer = (char*)username.c_str();
    bind_param[1].buffer_length = username.length();
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

    // 检查受影响的行数
    if (mysql_stmt_affected_rows(stmt) == 0) {
        cerr << "No rows were updated. Check if the username exists." << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return true;
}

// 更新用户最新登录时间
bool sql_blog_tool::update_last_login_time(int user_id)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII conn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    const char* query = "UPDATE user SET last_login_time = NOW() WHERE userId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
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

// 插入新的博客点赞
bool sql_blog_tool::insert_new_blog_like(int userid, int blogid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句
    const char* query = "INSERT INTO blog_likes (user_id, blog_id) VALUES (?, ?)";

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
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    int user_id = userid;
    int blog_id = blogid;

    // 绑定 user_id
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    // 绑定 blog_id
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&blog_id;
    bind[1].is_null = 0;
    bind[1].length = 0;

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

// 删除博客点赞
bool sql_blog_tool::remove_blog_like(int user_id, int blog_id)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句
    const char* query = "DELETE FROM blog_likes WHERE user_id = ? AND blog_id = ?";

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
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    // 绑定 user_id
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    // 绑定 blog_id
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&blog_id;
    bind[1].is_null = 0;
    bind[1].length = 0;

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行删除操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return true;
}

// 获取当前博客的点赞总数
int sql_blog_tool::get_blog_likes_count(int blog_id)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句，查询点赞数
    const char* query = "SELECT COUNT(*) FROM blog_likes WHERE blog_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    // 准备SQL语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    // 绑定 blog_id
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&blog_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 执行查询操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    int like_count = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&like_count;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 清理
    mysql_stmt_close(stmt);

    return like_count;
}

// 检测用户是否已经对该博客点赞
bool sql_blog_tool::is_user_liked_blog(int user_id, int blog_id) {

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    const char* query = "SELECT COUNT(*) FROM blog_likes WHERE user_id = ? AND blog_id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (char*)&blog_id;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    int like_count = 0;
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&like_count;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);

    return like_count > 0;
}

// 获取用户浏览量
int sql_blog_tool::get_view_count_by_userid(int userid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    const char* query = "SELECT SUM(view_count) FROM blog WHERE userId = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&userid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    int view_count = 0;
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&view_count;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    mysql_stmt_close(stmt);

    return view_count;
}

// 获取用户获赞总数
int sql_blog_tool::get_blog_liked_count_by_userid(int userid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");
    
    const char* query = "SELECT COUNT(*) AS total_likes FROM blog_likes JOIN blog ON blog_likes.blog_id = blog.blogId WHERE blog.userId = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&userid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    int like_count = 0;
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&like_count;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }   

    mysql_stmt_close(stmt);

    return like_count;
}



// 获取当前博客的评论总数
int sql_blog_tool::get_blog_comments_count(int blogid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句，查询点赞数
    const char* query = "SELECT COUNT(*) FROM blog_comments WHERE blog_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    // 准备SQL语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    // 绑定 blog_id
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&blogid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 执行查询操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    int comment_count = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = (char*)&comment_count;
    result_bind[0].is_null = 0;
    result_bind[0].length = 0;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 清理
    mysql_stmt_close(stmt);

    return comment_count;
}

// 删除指定id的评论
bool sql_blog_tool::delete_comment_by_commentid(int commentid)
{
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);

    // 构建SQL删除语句
    string query = "DELETE FROM blog_comments WHERE comment_id = ?";
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
    bind_param.buffer = (char*)&commentid;

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

// 批量删除评论
bool sql_blog_tool::batch_delete_comments(const vector<int> &commentIds) {
    if (commentIds.empty()) return false;

    connection_pool *connpool = connection_pool::GetInstance();
    MYSQL *mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    string query = "DELETE FROM blog_comments WHERE comment_id IN (";
    for (size_t i = 0; i < commentIds.size(); ++i) {
        query += to_string(commentIds[i]);
        if (i < commentIds.size() - 1) query += ",";
    }
    query += ")";

    if (mysql_query(mysql, query.c_str())) {
        cerr << "MySQL 执行错误: " << mysql_error(mysql) << endl;
        return false;
    }

    int affectedRows = mysql_affected_rows(mysql);
    return affectedRows;
}

// 通过评论id获取评论详情
// 通过评论id获取评论详情
Comments sql_blog_tool::get_comment_by_commentId(int commentId)
{
    // 初始化一个空的Comments对象
    Comments comment;
    
    // 获取连接池实例并获取 MySQL 连接
    connection_pool* pool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII con(&mysql, pool);
    
    // 设置字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 构建SQL查询语句
    string query = "SELECT comment_id, blog_id, username, content, comment_time FROM blog_comments WHERE comment_id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << endl;
        return comment;
    }

    // 预处理 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comment;
    }

    // 准备绑定参数
    MYSQL_BIND bind_param;
    memset(&bind_param, 0, sizeof(bind_param));
    bind_param.buffer_type = MYSQL_TYPE_LONG;
    bind_param.buffer = (char*)&commentId;

    // 绑定参数到语句
    if (mysql_stmt_bind_param(stmt, &bind_param)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comment;
    }

    // 执行查询语句
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comment;
    }

    // 准备结果集绑定变量
    int comment_id, blog_id;
    char username[128] = {};
    char content[4096] = {}; // 评论内容可能较长
    char comment_time[64] = {};
    
    // 长度和空值标志
    unsigned long username_length, content_length, comment_time_length;
    my_bool username_is_null, content_is_null, comment_time_is_null;
    
    // 设置结果集绑定
    MYSQL_BIND result_bind[5] = {};
    
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &comment_id;
    
    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &blog_id;
    
    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = username;
    result_bind[2].buffer_length = sizeof(username);
    result_bind[2].length = &username_length;
    result_bind[2].is_null = &username_is_null;
    
    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content;
    result_bind[3].buffer_length = sizeof(content);
    result_bind[3].length = &content_length;
    result_bind[3].is_null = &content_is_null;
    
    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = comment_time;
    result_bind[4].buffer_length = sizeof(comment_time);
    result_bind[4].length = &comment_time_length;
    result_bind[4].is_null = &comment_time_is_null;

    // 绑定结果集
    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comment;
    }

    // 存储结果集
    if (mysql_stmt_store_result(stmt)) {
        cerr << "mysql_stmt_store_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return comment;
    }

    // 检查是否有结果
    if (mysql_stmt_num_rows(stmt) == 0) {
        // 没有找到记录
        mysql_stmt_close(stmt);
        return comment;
    }

    // 提取结果
    int fetch_result = mysql_stmt_fetch(stmt);
    if (fetch_result == 0) {
        // 设置评论对象的字段
        comment.set_comment_id(comment_id);
        comment.set_blog_id(blog_id);
        
        if (!username_is_null) {
            comment.set_username(string(username, username_length));
        }
        
        if (!content_is_null) {
            comment.set_content(string(content, content_length));
        }
        
        if (!comment_time_is_null) {
            comment.set_comment_time(string(comment_time, comment_time_length));
        }
    } else if (fetch_result != MYSQL_NO_DATA) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
    }

    // 释放语句句柄
    mysql_stmt_close(stmt);
    return comment;
}

// 更新评论信息
bool sql_blog_tool::update_comment_by_commentid(int commentid, Comments comment)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 设置字符集
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    std::string query = "UPDATE blog_comments SET ";
    std::string condition = " WHERE comment_id = ?";

    std::vector<std::string> fields;
    std::vector<std::string> values;
    std::vector<MYSQL_BIND> binds;

    // 检查并添加需要更新的字段
    if (!comment.get_content().empty()) {
        fields.push_back("content = ?");
        values.push_back(comment.get_content());
    }
    
    if (!comment.get_username().empty()) {
        fields.push_back("username = ?");
        values.push_back(comment.get_username());
    }
    
    if (comment.get_blog_id() > 0) {
        fields.push_back("blog_id = ?");
        values.push_back(std::to_string(comment.get_blog_id()));
    }
    
    // 添加更新时间字段
    fields.push_back("comment_time = NOW()");
    
    if (fields.empty()) {
        return true; // 没有需要更新的字段，直接返回
    }

    // 手动拼接 SQL 语句
    query += fields[0];
    for (size_t i = 1; i < fields.size(); ++i) {
        query += ", " + fields[i];
    }
    query += condition;

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed: " << mysql_error(mysql) << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数 (注意：对于comment_time = NOW()，我们不需要添加绑定)
    int bindCount = values.size();
    binds.resize(bindCount + 1); // +1 for commentid
    memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));

    // 绑定字段值
    for (size_t i = 0; i < values.size(); i++) {
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = (char*)values[i].c_str();
        binds[i].buffer_length = values[i].length();
    }

    // 绑定 commentid
    binds[bindCount].buffer_type = MYSQL_TYPE_LONG;
    binds[bindCount].buffer = &commentid;

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查是否真的更新了记录
    if (mysql_stmt_affected_rows(stmt) == 0) {
        // 没有找到对应的评论ID，但不算错误
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 增加评论到文章
int sql_blog_tool::add_comment_to_article(string username, int articleid, string content)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    const char* query = "INSERT INTO blog_comments (blog_id, username, content, comment_time) VALUES (?, ?, ?, NOW())";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&articleid;
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)username.c_str();
    bind[1].buffer_length = username.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)content.c_str();
    bind[2].buffer_length = content.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取新插入评论的ID
    int comment_id = mysql_stmt_insert_id(stmt);
    
    mysql_stmt_close(stmt);
    return comment_id;
}

// 增加博客的浏览量
bool sql_blog_tool::increase_blog_view_count(int blogid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句，增加浏览量
    const char* query = "UPDATE blog SET view_count = view_count + 1 WHERE blogId = ?";

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
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&blogid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    // 绑定参数到预处理语句
    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行更新操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return true;
}

// 增加文章总数（+1）
bool sql_blog_tool::increase_article_count(int user_id)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    const char* query = "UPDATE user SET article_count = article_count + 1 WHERE userId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 文章总数-1
bool sql_blog_tool::decrease_article_count(int user_id)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 确保 article_count 不小于 0
    const char* query = "UPDATE user SET article_count = GREATEST(article_count - 1, 0) WHERE userId = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&user_id;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 用户注册
bool sql_blog_tool::user_register(string username, string password, string email, string avatar)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 插入语句
    const char* query = "INSERT INTO user (username, password, email, avatar) VALUES (?, ?, ?, ?)";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)password.c_str();
    bind[1].buffer_length = password.length();

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)email.c_str();
    bind[2].buffer_length = email.length();

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)avatar.c_str();
    bind[3].buffer_length = avatar.length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 管理员添加用户
bool sql_blog_tool::add_user_from_admin(User user)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    // 构造 SQL 插入语句
    const char* query = "INSERT INTO user (username, password, email, status, description, register_time) VALUES (?, ?, ?, ?, ?, NOW());";

    // 预处理 SQL 查询
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));

    // 绑定 username
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)user.get_username().c_str();
    bind[0].buffer_length = user.get_username().length();

    // 绑定 password
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char*)user.get_password().c_str();
    bind[1].buffer_length = user.get_password().length();

    // 绑定 email
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (char*)user.get_eamil().c_str();
    bind[2].buffer_length = user.get_eamil().length();

    // 绑定 status
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)user.get_status().c_str();
    bind[3].buffer_length = user.get_status().length();

    // 绑定 status
    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = (char*)user.get_description().c_str();
    bind[4].buffer_length = user.get_description().length();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 语句
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 关闭语句
    mysql_stmt_close(stmt);

    return true;
}

// 删除用户
bool sql_blog_tool::delete_user_by_userid(int userid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4' COLLATE 'utf8mb4_unicode_ci'");

    // 准备 SQL 语句
    const char* query = "DELETE FROM user WHERE userId = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &userid;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行删除
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查是否有行被删除
    my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);

    // 如果删除成功且有行被删除，返回 true
    return (affected_rows > 0);
}

// 更新用户信息
bool sql_blog_tool::update_user_by_userid(int userid, User user) {
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    std::string query = "UPDATE user SET ";
    std::string condition = " WHERE userId = ?";

    std::vector<std::string> fields;
    std::vector<std::string> values;
    std::vector<MYSQL_BIND> binds;

    if (!user.get_username().empty()) {
        fields.push_back("username = ?");
        values.push_back(user.get_username());
    }
    if (!user.get_password().empty()) {
        fields.push_back("password = ?");
        values.push_back(user.get_password());
    }
    if (!user.get_eamil().empty()) {
        fields.push_back("email = ?");
        values.push_back(user.get_eamil());
    }
    if (!user.get_status().empty()) {
        fields.push_back("status = ?");
        values.push_back(user.get_status());
    }
    if (!user.get_description().empty()) {
        fields.push_back("description = ?");
        values.push_back(user.get_description());
    }

    if (fields.empty()) {
        return true; // 没有需要更新的字段，直接返回
    }

    // 手动拼接 SQL 语句
    query += fields[0];
    for (size_t i = 1; i < fields.size(); ++i) {
        query += ", " + fields[i];
    }
    query += condition;

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    binds.resize(values.size() + 1);
    memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));

    for (size_t i = 0; i < values.size(); i++) {
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = (char*)values[i].c_str();
        binds[i].buffer_length = values[i].length();
    }

    // 绑定 userid
    binds[values.size()].buffer_type = MYSQL_TYPE_LONG;
    binds[values.size()].buffer = &userid;

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 更新文章信息
bool sql_blog_tool::update_blog_by_blogid(int blogid, Blog blog) {
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;

    // 从连接池中取出一个连接
    connectionRAII conn(&mysql, connpool);

    std::string query = "UPDATE blog SET ";
    std::string condition = " WHERE blogId = ?";

    std::vector<std::string> fields;
    std::vector<std::string> values;
    std::vector<MYSQL_BIND> binds;

    // 检查并添加需要更新的字段
    if (!blog.get_blog_title().empty()) {
        fields.push_back("title = ?");
        values.push_back(blog.get_blog_title());
    }
    
    // 对于分类ID，我们需要检查是否有有效值
    if (blog.get_category_id() > 0) {
        fields.push_back("category_id = ?");
        values.push_back(std::to_string(blog.get_category_id()));
    }
    
    if (!blog.get_blog_content().empty()) {
        fields.push_back("content = ?");
        values.push_back(blog.get_blog_content());
    }
    
    // 可以根据需要添加更多字段
    // 比如更新时间
    fields.push_back("updatedAt = NOW()");
    
    if (fields.empty()) {
        return true; // 没有需要更新的字段，直接返回
    }

    // 手动拼接 SQL 语句
    query += fields[0];
    for (size_t i = 1; i < fields.size(); ++i) {
        query += ", " + fields[i];
    }
    query += condition;

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        std::cerr << "mysql_stmt_init() failed" << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        std::cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数 (注意：对于updateTime = NOW()，我们不需要添加绑定)
    int bindCount = values.size();
    binds.resize(bindCount + 1); // +1 for blogid
    memset(binds.data(), 0, binds.size() * sizeof(MYSQL_BIND));

    for (size_t i = 0; i < values.size(); i++) {
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = (char*)values[i].c_str();
        binds[i].buffer_length = values[i].length();
    }

    // 绑定 blogid
    binds[bindCount].buffer_type = MYSQL_TYPE_LONG;
    binds[bindCount].buffer = &blogid;

    if (mysql_stmt_bind_param(stmt, binds.data())) {
        std::cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 根据管理员昵称获取对应的密码
string sql_blog_tool::get_admin_password_by_username(string username)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 预处理SQL语句，查询管理员密码
    const char* query = "SELECT password FROM admin WHERE username = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return "";
    }

    // 准备SQL语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].buffer_length = username.length();
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 执行查询操作
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    char password[256];
    memset(password, 0, sizeof(password));
    unsigned long length = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = password;
    result_bind[0].buffer_length = sizeof(password);
    result_bind[0].is_null = 0;
    result_bind[0].length = &length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 清理
    mysql_stmt_close(stmt);

    return string(password);
}

// 获取博客的所有分类
vector<Categories> sql_blog_tool::get_cotegories_all()
{
    connection_pool* connpool = connection_pool::GetInstance();

	vector<Categories> categories;
	MYSQL* mysql = NULL;
	// 从数据库连接池中取出一个连接
	connectionRAII mysqlcon(&mysql, connpool);

	// 设置连接字符集
	mysql_query(mysql, "SET NAMES 'utf8mb4'");

	// 查询博客数据
	if(mysql_query(mysql, "SELECT id, name, description, created_at, updated_at FROM categories")){
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
        Categories categorie;
        categorie.set_id(stoi(row[0]));
        categorie.set_name(row[1]);
        categorie.set_description(row[2]);
        categorie.set_created_at(row[3]);
        categorie.set_updated_at(row[4]);

        categories.push_back(categorie);
	}

	// 释放结果集
	mysql_free_result(result);
	return categories;
}

// 根据分类 ID 获取分类名称
string sql_blog_tool::get_cotegoriename_by_cotegorieid(int cotegorieid)
{
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // SQL 查询语句
    const char* query = "SELECT name FROM categories WHERE id = ?";

    // 初始化预处理语句
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return "";
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char*)&cotegorieid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    char category_name[256];
    memset(category_name, 0, sizeof(category_name));
    unsigned long length = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = category_name;
    result_bind[0].buffer_length = sizeof(category_name);
    result_bind[0].is_null = 0;
    result_bind[0].length = &length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 获取结果
    if (mysql_stmt_fetch(stmt)) {
        cerr << "mysql_stmt_fetch() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return "";
    }

    // 关闭语句
    mysql_stmt_close(stmt);

    return string(category_name);
}

// 根据分类id获取分类信息
Categories sql_blog_tool::get_categorie_by_categorieid(int categorieId)
{
    Categories categorie;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 查询语句
    string query = "SELECT id, name, description, created_at, updated_at FROM categories WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categorie;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 绑定查询参数
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &categorieId;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 执行 SQL 查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 绑定结果
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 获取数据
    if (mysql_stmt_fetch(stmt) == 0) {
        categorie.set_id(id);

        if (!name_is_null) {
            categorie.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            categorie.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }
    }

    mysql_stmt_close(stmt);
    return categorie;
}

// 根据分类名称获取分类id
int sql_blog_tool::get_category_id_by_name(const string& categorieName)
{
    Categories categorie;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 查询语句
    string query = "SELECT id FROM categories WHERE name = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定查询参数
    MYSQL_BIND bind[1] = {};
    unsigned long name_length = categorieName.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)categorieName.c_str();
    bind[0].buffer_length = name_length;
    bind[0].length = &name_length;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 执行 SQL 查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定结果
    MYSQL_BIND result_bind[1] = {};
    int id;
    my_bool is_null;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;
    result_bind[0].is_null = &is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取数据
    if (mysql_stmt_fetch(stmt) == 0 && !is_null) {
        mysql_stmt_close(stmt);
        return id;
    }

    mysql_stmt_close(stmt);
    return -1;  // 未找到对应的分类
}

// 根据分类名称获取分类信息
Categories sql_blog_tool::get_categorie_by_name(const string& categorieName)
{
    Categories categorie;
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 查询语句
    string query = "SELECT id, name, description, created_at, updated_at FROM categories WHERE name = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return categorie;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 绑定查询参数
    MYSQL_BIND bind[1] = {};
    unsigned long name_length = categorieName.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)categorieName.c_str();
    bind[0].buffer_length = name_length;
    bind[0].length = &name_length;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 执行 SQL 查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 绑定结果
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length_result, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length_result;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return categorie;
    }

    // 获取数据
    if (mysql_stmt_fetch(stmt) == 0) {
        categorie.set_id(id);

        if (!name_is_null) {
            categorie.set_name(string(name, name_length_result));
        }
        if (!description_is_null) {
            categorie.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            categorie.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            categorie.set_updated_at(string(updated_at, updated_at_length));
        }
    }

    mysql_stmt_close(stmt);
    return categorie;
}

// 添加分类
bool sql_blog_tool::add_categorie(Categories categorie)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 插入语句
    string query = "INSERT INTO categories (name, description, created_at, updated_at) VALUES (?, ?, NOW(), NOW())";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 获取分类信息
    string name = categorie.get_name();
    string description = categorie.get_description();

    // 绑定插入参数
    MYSQL_BIND bind[2] = {};
    
    unsigned long name_length = name.length();
    unsigned long description_length = description.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = name_length;
    bind[0].length = &name_length;
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)description.c_str();
    bind[1].buffer_length = description_length;
    bind[1].length = &description_length;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 插入
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查影响的行数
    if (mysql_stmt_affected_rows(stmt) != 1) {
        cerr << "Failed to insert category" << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

// 添加标签
// 添加标签
bool sql_blog_tool::add_tag(Tags tag)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 插入语句
    string query = "INSERT INTO tags (name, description, created_at, updated_at) VALUES (?, ?, NOW(), NOW())";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    // 这里缺少了mysql_stmt_prepare()调用！
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 获取标签信息
    string name = tag.get_name();
    string description = tag.get_description();

    // 绑定插入参数
    MYSQL_BIND bind[2] = {};
    
    unsigned long name_length = name.length();
    unsigned long description_length = description.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = name_length;
    bind[0].length = &name_length;
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)description.c_str();
    bind[1].buffer_length = description_length;
    bind[1].length = &description_length;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 插入
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查影响的行数
    if (mysql_stmt_affected_rows(stmt) != 1) {
        cerr << "Failed to insert tag" << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool sql_blog_tool::update_categorie_by_categorieid(int categorieid, Categories categorie)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 获取原始分类信息
    Categories original_categorie = get_categorie_by_categorieid(categorieid);
    if (original_categorie.get_id() == 0) {
        cerr << "No category found with ID: " << categorieid << endl;
        return false;
    }

    // 检查需要更新哪些字段
    string name = categorie.get_name().empty() ? original_categorie.get_name() : categorie.get_name();
    string description = categorie.get_description().empty() ? original_categorie.get_description() : categorie.get_description();

    // 准备 SQL 更新语句
    string query = "UPDATE categories SET name = ?, description = ?, updated_at = NOW() WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定更新参数
    MYSQL_BIND bind[3] = {};
    
    unsigned long name_length = name.length();
    unsigned long description_length = description.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = name_length;
    bind[0].length = &name_length;
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)description.c_str();
    bind[1].buffer_length = description_length;
    bind[1].length = &description_length;
    
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &categorieid;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 更新
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return true;
}

bool sql_blog_tool::delete_categorie_by_categorieid(int categorieid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 删除语句
    string query = "DELETE FROM categories WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定删除参数
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &categorieid;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 删除
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查影响的行数
    my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);
    
    // 如果影响的行数为0，可能是因为ID不存在
    if (affected_rows == 0) {
        cerr << "No category found with ID: " << categorieid << endl;
        return false;
    }

    return true;
}

// 删除标签
bool sql_blog_tool::delete_tag_by_tagid(int tagid)
{
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备 SQL 删除语句
    string query = "DELETE FROM tags WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定删除参数
    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &tagid;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行 SQL 删除
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 检查影响的行数
    my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);

    // 如果影响的行数为0，可能是因为ID不存在
    if (affected_rows == 0) {
        cerr << "No tag found with ID: " << tagid << endl;
        return false;
    }

    return true;
}

// 通过博客id获取该博客的标签名称
vector<string> sql_blog_tool::get_tags_by_blogid(int blogid)
{
    vector<string> tags;

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 查询博客对应的所有标签名称
    const char* query = 
        "SELECT tags.name FROM tags "
        "JOIN blog_tags ON tags.id = blog_tags.tag_id "
        "WHERE blog_tags.blog_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tags;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &blogid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 绑定结果
    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    char tag_name[256];
    memset(tag_name, 0, sizeof(tag_name));
    unsigned long length = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_STRING;
    result_bind[0].buffer = tag_name;
    result_bind[0].buffer_length = sizeof(tag_name);
    result_bind[0].is_null = 0;
    result_bind[0].length = &length;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tags;
    }

    // 获取查询结果
    while (mysql_stmt_fetch(stmt) == 0) {
        tags.push_back(string(tag_name));
    }

    // 清理
    mysql_stmt_close(stmt);

    return tags;
}

// 根据博客id删除旧的标签关联
bool sql_blog_tool::delete_blog_tags(int blogid)
{
    if (blogid <= 0) {
        return false;
    }

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备删除SQL语句
    const char* query = "DELETE FROM blog_tags WHERE blog_id = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &blogid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行删除
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 清理
    mysql_stmt_close(stmt);

    return true;
}

// 根据标签名称获取标签ID
int sql_blog_tool::get_tag_id_by_name(string tag)
{
    if (tag.empty()) {
        return -1;
    }

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备查询SQL语句
    const char* query = "SELECT id FROM tags WHERE name = ?";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    unsigned long tag_len = tag.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)tag.c_str();
    bind[0].buffer_length = tag_len;
    bind[0].length = &tag_len;
    bind[0].is_null = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定结果集
    MYSQL_BIND result[1];
    memset(result, 0, sizeof(result));
    
    int tag_id = 0;
    my_bool is_null;

    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &tag_id;
    result[0].is_null = &is_null;

    if (mysql_stmt_bind_result(stmt, result)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 存储结果集
    if (mysql_stmt_store_result(stmt)) {
        cerr << "mysql_stmt_store_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取查询结果
    int found = 0;
    if (mysql_stmt_fetch(stmt) == 0) {
        if (!is_null) {
            found = 1;
        }
    }

    // 清理
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    // 如果找到标签，返回标签ID，否则返回-1
    return found ? tag_id : -1;
}

// 根据标签名称获取标签信息
Tags sql_blog_tool::get_tag_by_tagname(const string& tagname)
{
    Tags tag;
    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备查询SQL语句 - 明确指定需要的列而非使用 *
    string query = "SELECT id, name, description, created_at, updated_at FROM tags WHERE name = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return tag;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.length())) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tag;
    }

    // 绑定参数
    MYSQL_BIND bind[1] = {};
    unsigned long tag_len = tagname.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)tagname.c_str();
    bind[0].buffer_length = tag_len;
    bind[0].length = &tag_len;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tag;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tag;
    }

    // 绑定结果集
    int id;
    char name[128] = {};
    char description[512] = {};
    char created_at[64] = {};
    char updated_at[64] = {};

    unsigned long name_length, description_length, created_at_length, updated_at_length;
    my_bool name_is_null, description_is_null, created_at_is_null, updated_at_is_null;

    MYSQL_BIND result_bind[5] = {};
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_length;
    result_bind[1].is_null = &name_is_null;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = description;
    result_bind[2].buffer_length = sizeof(description);
    result_bind[2].length = &description_length;
    result_bind[2].is_null = &description_is_null;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at;
    result_bind[3].buffer_length = sizeof(created_at);
    result_bind[3].length = &created_at_length;
    result_bind[3].is_null = &created_at_is_null;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at;
    result_bind[4].buffer_length = sizeof(updated_at);
    result_bind[4].length = &updated_at_length;
    result_bind[4].is_null = &updated_at_is_null;

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        cerr << "mysql_stmt_bind_result() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return tag;
    }

    // 获取数据
    if (mysql_stmt_fetch(stmt) == 0) {
        tag.set_id(id);

        if (!name_is_null) {
            tag.set_name(string(name, name_length));
        }
        if (!description_is_null) {
            tag.set_description(string(description, description_length));
        }
        if (!created_at_is_null) {
            tag.set_created_at(string(created_at, created_at_length));
        }
        if (!updated_at_is_null) {
            tag.set_updated_at(string(updated_at, updated_at_length));
        }
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return tag;
}

// 创建标签
int sql_blog_tool::create_tag(string tag)
{
    if (tag.empty()) {
        return -1;
    }

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备插入SQL语句
    const char* query = "INSERT INTO tags (name) VALUES (?)";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return -1;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    unsigned long tag_len = tag.length();
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)tag.c_str();
    bind[0].buffer_length = tag_len;
    bind[0].length = &tag_len;
    bind[0].is_null = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        cerr << "mysql_stmt_execute() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    // 获取插入ID
    int new_tag_id = mysql_stmt_insert_id(stmt);
    
    // 清理
    mysql_stmt_close(stmt);

    return new_tag_id;
}

bool sql_blog_tool::add_blog_tag(int blogid, int tagid)
{
    if (blogid <= 0 || tagid <= 0) {
        return false;
    }

    // 获取数据库连接池实例
    connection_pool* connpool = connection_pool::GetInstance();
    MYSQL* mysql = nullptr;
    connectionRAII mysqlconn(&mysql, connpool);

    // 设置连接字符集，防止乱码
    mysql_query(mysql, "SET NAMES 'utf8mb4'");

    // 准备插入SQL语句
    const char* query = "INSERT INTO blog_tags (blog_id, tag_id) VALUES (?, ?)";

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt) {
        cerr << "mysql_stmt_init() failed" << endl;
        return false;
    }

    // 准备 SQL 语句
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        cerr << "mysql_stmt_prepare() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &blogid;
    bind[0].is_null = 0;
    bind[0].length = 0;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &tagid;
    bind[1].is_null = 0;
    bind[1].length = 0;

    if (mysql_stmt_bind_param(stmt, bind)) {
        cerr << "mysql_stmt_bind_param() failed: " << mysql_stmt_error(stmt) << endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        // 检查是否是因为唯一约束冲突导致的错误（可能已经存在相同的关联）
        if (mysql_stmt_errno(stmt) == 1062) { // 1062是MySQL中唯一约束冲突的错误码
            mysql_stmt_close(stmt);
            return true; // 已存在视为成功
        }
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

void User::set_status(string status)
{
    this->m_status = status;
}

string User::get_status()
{
    return this->m_status;
}

void User::set_last_login_time(string last_login_time)
{
    this->last_login_time = last_login_time;
}

string User::get_last_login_time()
{
    return this->last_login_time;
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

void Blog::set_category_id(int category_id)
{
    this->m_category_id = category_id;
}

int Blog::get_category_id()
{
    return this->m_category_id;
}

void Blog::set_updatedAt(string updatedAt)
{
    this->m_updatedAt = updatedAt;
}

string Blog::get_updatedAt()
{
    return this->m_updatedAt;
}

void Blog::set_views(int views)
{
    this->views = views;
}

int Blog::get_views()
{
    return this->views;
}

void Blog::set_thumbnail(string thumbnail)
{
    this->thumbnail = thumbnail;
}

string Blog::get_thumbnail()
{
    return this->thumbnail;
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


/*
 *下面的内容为博客点赞类
*/
void Blog_like::set_like_id(int like_id)
{
    this->m_like_id = like_id;
}

int Blog_like::get_like_id()
{
    return this->m_like_id;
}

void Blog_like::set_user_id(int user_id)
{
    this->m_user_id = user_id;
}

int Blog_like::get_user_id()
{
    return this->m_user_id;
}

void Blog_like::set_blog_id(int blog_id)
{
    this->m_blog_id = blog_id;
}

int Blog_like::get_blog_id()
{
    return this->m_blog_id;
}

void Blog_like::set_like_time(string like_time)
{
    this->m_like_time = like_time;
}

string Blog_like::get_like_time()
{
    return this->m_like_time;
}

void Categories::set_id(int id)
{
    this->m_id = id;
}

int Categories::get_id()
{
    return this->m_id;
}

void Categories::set_name(string name)
{
    this->m_name = name;
}

string Categories::get_name()
{
    return this->m_name;
}

void Categories::set_description(string description)
{
    this->m_description = description;
}

string Categories::get_description()
{
    return this->m_description;
}

void Categories::set_created_at(string created_at)
{
    this->m_created_at = created_at;
}

string Categories::get_created_at()
{
    return this->m_created_at;
}

void Categories::set_updated_at(string updated_at)
{
    this->m_updated_at = updated_at;
}

string Categories::get_updated_at()
{
    return this->m_updated_at;
}

void Categories::set_blog_count(int blog_count)
{
    this->blog_count = blog_count;
}

int Categories::get_blog_count()
{
    return this->blog_count;
}


/*
 *下面的内容为标签类
*/
void Tags::set_id(int id)
{
    this->m_id = id;
}

int Tags::get_id()
{
    return this->m_id;
}

void Tags::set_name(string name)
{
    this->m_name = name;
}

string Tags::get_name()
{
    return this->m_name;
}

void Tags::set_description(string description)
{
    this->m_description = description;
}

string Tags::get_description()
{
    return this->m_description;
}

void Tags::set_created_at(string created_at)
{
    this->m_created_at = created_at;
}

string Tags::get_created_at()
{
    return this->m_created_at;
}

void Tags::set_updated_at(string updated_at)
{
    this->m_updated_at = updated_at;
}

string Tags::get_updated_at()
{
    return this->m_updated_at;
}

/*
 *下面的内容为博客标签关联类
*/
void Blog_tags::set_blog_id(int blog_id)
{
    this->m_blog_id = blog_id;
}

int Blog_tags::get_blog_id()
{
    return this->m_blog_id;
}

void Blog_tags::set_tag_id(int tag_id)
{
    this->m_tag_id = tag_id;
}

int Blog_tags::get_tag_id()
{
    return this->m_tag_id;
}
