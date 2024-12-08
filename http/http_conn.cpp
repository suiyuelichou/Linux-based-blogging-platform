#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <string>

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *ok_302_title = "Found";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;  //将用户名和密码存入哈希表中，减少数据库的检索损耗

// 从数据库中获取用户的用户名和密码存入map
void http_conn::initmysql_result(connection_pool *connPool)
{
    MYSQL *mysql = NULL;
    //先从连接池中取一个连接
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据
    if (mysql_query(mysql, "SELECT username,password FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;  //新的事件配置
    event.data.fd = fd;     //data.fd：保存fd（需要监听的对象） events：指定需要监听的事件类型及触发模式

    if (1 == TRIGMode)  //若TRIGMode为1，设置为边缘触发(EPOLLET)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;    //EPOLLONESHOT：一个文件描述符的事件在触发一次后会被自动移除
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;  //EPOLLRDHUP：表示对方关闭连接或半关闭，用于检测对方是否关闭了socket连接

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);  //EPOLL_CTL_MOD：修改已经在epoll实例中的文件描述符的监听事件
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

//初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

//初始化新接受的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    islogin = false;
    current_username = "";

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')   //回车符
        {
            if ((m_checked_idx + 1) == m_read_idx)  // 若'\r'后面没有更多字符，表示行未结束
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    // 检查索引是否超出了缓冲区的最大大小
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        // m_read_buf + m_read_idx ：计算出缓冲区中实际写入数据的位置。
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // 例：GET / HTTP/1.1   若不存在空格或制表符，说明后面没有目标url
    m_url = strpbrk(text, " \t");

    if (!m_url)
    {
        return BAD_REQUEST;
    }

    *m_url++ = '\0';
    char *method = text;

    // 暂时只实现了get和post请求
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }else if(strcasecmp(method, "DELETE") == 0){
        m_method = DELETE;
    }
    else if(strcasecmp(method, "PATCH") == 0){
        m_method = PATCH;
    }
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");

    if (!m_version)
        return BAD_REQUEST;

    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    //当url为/时，显示判断界面
    if (strlen(m_url) == 1)     // 若长度为1，说明请求的是'/'
        strcat(m_url, "blog_home.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//解析http请求的请求头（一行一行传进来）
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')    // 若是空行，说明请求头已结束（请求头和请求体之间是一行空行）
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else if(strncasecmp(text, "Cookie:", 7) == 0){
        text += 7;
        text += strspn(text, " \t");
        cookie.parseCookieHeader(text);     // 解析并存储cookie字符串
    }
    else    // 若某一行的请求头不属于上述之一，则会重复写入日志，并在前面添加报警
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求体是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 解析http请求体内容并存储
unordered_map<string, string> http_conn::parse_post_data(const string& body){
    unordered_map<string, string> post_data;
    string::size_type start = 0;
    string::size_type pos;

    while((pos = body.find('&', start)) != string::npos){
        string pair = body.substr(start, pos - start);
        string::size_type equalPos = pair.find('=');

        if(equalPos != string::npos){
            string key = pair.substr(0, equalPos);
            string value = pair.substr(equalPos + 1);
            post_data[key] = value;
        }
        start = pos + 1;
    }

    // 处理最后一个键值对
    string lastPair = body.substr(start);
    string::size_type lastEqualPost = lastPair.find('=');
    if(lastEqualPost != string::npos){
        string key = lastPair.substr(0, lastEqualPost);
        string value = lastPair.substr(lastEqualPost + 1);
        post_data[key] = value;
    }

    return post_data;
}

// URL解码函数
string http_conn::url_decode(const string &str)
{
    std::ostringstream decoded;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream is(str.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                decoded << static_cast<char>(value);
                i += 2;
            }
        } else if (str[i] == '+') {
            decoded << ' ';
        } else {
            decoded << str[i];
        }
    }
    return decoded.str();
}

// 处理从客户端读取的HTTP请求（判断请求是否成功）
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    // 处理读取的 HTTP 请求内容
    // 该循环负责解析 HTTP 请求的不同部分，包括请求行、请求头和请求体。
    // 根据当前的状态（请求行、请求头、请求体），逐行读取并解析输入数据。
    // - 如果当前状态是 CHECK_STATE_CONTENT，且行解析成功，继续处理请求体。
    // - 否则，调用 parse_line() 解析下一行，并根据当前状态调用相应的解析函数。
    // - 在解析过程中，根据返回值判断请求的有效性，处理请求或返回错误状态。
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();  // 获取当前行内容，并将指针赋值给text
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);   // 将当前行写入日志

        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:   // 转去解析请求行
            {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:    // 转去解析请求头
            {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:   // 转去解析请求体
            {
                ret = parse_content(text);
                if (ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
            }
    }
    return NO_REQUEST;
}

//当请求解析成功，对客户端请求作出响应
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    const char *p = strrchr(m_url, '/');    //返回m_url中的最后一个'/'字符的位置
    char name[100], password[100];

    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        int i;
        for (i = 5; m_string[i] != '&'; ++i)    //？m_string存储的是请求头哪部分的数据
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //如果是注册，先检测数据库中是否有重名的
        if (*(p + 1) == '3')
        {
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())    //哈希表中不存在相同的用户名
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);   // 执行sql_insert中的语句
                users.insert(pair<string, string>(name, password));     //数据插入成功后，将用户名和密码也存入users中
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/blog_login.html");
                else
                    strcpy(m_url, "/blog_registerError.html");
            }
            else
                strcpy(m_url, "/blog_registerError.html");
        }
        else if (*(p + 1) == '2')   //如果是登录，直接判断
        {
            //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            if (users.find(name) != users.end() && users[name] == password){
                // 创建新会话
                // cookie.createSession(name);
                char *m_url_real = (char *)malloc(sizeof(char) * 200);
                try{
                    cookie.createSession(name);
                    current_username = name;
                    islogin = true;
                    strcpy(m_url, "/blog_user_home.html");
                    strcpy(m_url_real, "/blog_user_home.html");
                } catch(const runtime_error& e){    // 若当前用户已登录，再登录会返回错误页面(用户已登录)
                    strcpy(m_url, "/blog_loginErrorExit.html");
                    strcpy(m_url_real, "/blog_loginErrorExit.html");
                }

                strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
                free(m_url_real);
            }
            else{
                strcpy(m_url, "/blog_loginError.html");
                char *m_url_real = (char *)malloc(sizeof(char) * 200);
                strcpy(m_url_real, "/blog_loginError.html");
                strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
                free(m_url_real);
            }
            
            // 文件检查与映射
            if (stat(m_real_file, &m_file_stat) < 0)
                return NO_RESOURCE;

            if (!(m_file_stat.st_mode & S_IROTH))
                return FORBIDDEN_REQUEST;

            if (S_ISDIR(m_file_stat.st_mode))
                return BAD_REQUEST;     // 插眼

            int fd = open(m_real_file, O_RDONLY);   // 以只读模式打开文件，返回文件描述符fd
            m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);   //通过mmap将文件的内容映射到进程的地址空间中
            close(fd);
            return LOGIN_REQUEST;
        }

    }
    // 这里用来判断是否将用户新建的博客保存进数据库
    else if(cgi == 1 && strstr(m_url, "/blog") != nullptr){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            // 对请求体的内容进行解码
            string body = url_decode(m_string);

            // 获取请求体的内容
            auto post_data = parse_post_data(body);

            // 获取title和content字段
            string title = post_data["title"];
            string content = post_data["content"];

            // 获取userId
            sql_blog_tool tool;
            int userid = tool.get_userid(username);

            // 获取系统时间
            time_t now = time(nullptr);
            tm* local_time = localtime(&now);
            char buffer[100];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
            string postTime = string(buffer);

            Blog blog;
            blog.set_blog_title(title);
            blog.set_blog_content(content);
            blog.set_user_id(userid);
            blog.set_blog_postTime(postTime);

            tool.insert_blog(blog);

            return REDIRECT_USER_HOME;
        }
    }
    // 这里用来判断是否将用户修改的博客保存进数据库
    else if(m_method == PATCH && strstr(m_url, "/modify_blog?blogId=") != nullptr){
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blogId = atoi(blogIdStart + 7);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            // 对请求体的内容进行解码
            string body = url_decode(m_string);

            // 获取请求体的内容
            auto post_data = parse_post_data(body);

            // 获取blogId、title和content字段
            string blogId = post_data["blogId"];
            string title = post_data["title"];
            string content = post_data["content"];

            // 获取userId
            sql_blog_tool tool;
            int userid = tool.get_userid(username);
            int userId = tool.get_userid_by_blogid(stoi(blogId));

            // 判断用户修改的是不是自己的博客
            if(userid == userId){
                Blog blog;
                blog.set_blog_title(title);
                blog.set_blog_content(content);
                blog.set_blog_id(stoi(blogId));

                tool.modify_blog_by_blogid(blog);

                return REDIRECT_USER_HOME;
            }else{
                return BAD_REQUEST;
            }
        }
    }
    // 这里用来判断是否删除用户指定的博客
    else if(m_method == DELETE && strstr(m_url, "/delete_blog?blogId=") != nullptr){
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blogId = atoi(blogIdStart + 7);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            // 获取userId
            sql_blog_tool tool;
            int userid = tool.get_userid(username);
            int userId = tool.get_userid_by_blogid(blogId);

            // 判断用户删除的是不是自己的博客
            if(userid == userId){
                tool.delete_blog_by_blogid(blogId);
                return REDIRECT_USER_HOME;
            }else{
                return BAD_REQUEST;
            }
        }
    }
    // 这里用来判断是否修改用户密码
    else if(m_method == PATCH && strstr(m_url, "/update_password") != nullptr){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            // 获取userId
            sql_blog_tool tool;
            int userid = tool.get_userid(username);

            string body = url_decode(m_string);
            auto post_data = parse_post_data(body);

            string old_password = post_data["oldPassword"];
            string new_password = post_data["newPassword"];

            // 这里是用来检测用户输入的旧密码对不对
            if(users[username] == old_password){
                tool.modify_password_by_username(username, new_password);
                users[username] = new_password;
                return REDIRECT_USER_HOME;
            }else{
                return BAD_REQUEST;
            }
        }
    }

    if (strstr(m_url, "/get_blogs?")) {
        int page = 1, size = 20;
        char* pageParam = strstr(m_url, "page=");
        char* sizeParam = strstr(m_url, "size=");
        if (pageParam) {
            page = std::atoi(pageParam + 5);
            if (page <= 0) page = 1;
        }
        if (sizeParam) {
            size = std::atoi(sizeParam + 5);
            if (size <= 0 || size > 100) size = 20;
        }
        
        // 查询数据库
        sql_blog_tool tool;
        vector<Blog> blogs = tool.get_blogs_by_page(page, size);
        int totalCount = tool.get_total_blog_count();

        // 添加转义字符处理函数
        auto escapeJsonString = [](const string& input) {
            string output;
            for (char c : input) {
                switch (c) {
                    case '"': output += "\\\""; break;   // 双引号转义
                    case '\\': output += "\\\\"; break;  // 反斜杠转义
                    case '\b': output += "\\b"; break;   // 退格符
                    case '\f': output += "\\f"; break;   // 换页符
                    case '\n': output += "\\n"; break;   // 换行符
                    case '\r': output += "\\r"; break;   // 回车符
                    case '\t': output += "\\t"; break;   // 制表符
                    default:
                        // 处理其他不可打印字符
                        if (iscntrl(c)) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            output += buf;
                        } else {
                            output += c;
                        }
                }
            }
            return output;
        };

        // 构造 JSON 响应
        jsonData = "{";
        jsonData += "\"blogs\": [";
        for (int i = 0; i < blogs.size(); i++) {
            Blog blog = blogs[i];
            string escapedTitle = escapeJsonString(blog.get_blog_title());
            string escapedContent = escapeJsonString(blog.get_blog_content());
            jsonData += "{";
            jsonData += "\"blogId\": " + std::to_string(blog.get_blog_id()) + ",";
            jsonData += "\"title\": \"" + escapedTitle + "\",";
            jsonData += "\"content\": \"" + escapedContent + "\",";
            jsonData += "\"userId\": " + std::to_string(blog.get_user_id()) + ",";
            jsonData += "\"postTime\": \"" + blog.get_blog_postTime() + "\"";
            jsonData += "}";
            if (i < blogs.size() - 1) jsonData += ",";
        }
        jsonData += "],";
        jsonData += "\"totalCount\": " + std::to_string(totalCount);
        jsonData += "}";

        return BLOG_DATA;
    }
    // 处理访问 blog_detail.html 的情况
    else if (strstr(m_url, "/blog_detail.html") != nullptr) {
        // 直接返回 blog_detail.html 页面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/blog_detail.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    // 处理访问 blog_detail_user.html 的情况（和上面一样，只是把游客和用户分开了）
    else if (strstr(m_url, "/blog_detail_user.html") != nullptr) {
        // 直接返回 blog_detail.html 页面
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/blog_detail_user.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }    
    // 处理博客详情数据请求
    else if (strstr(m_url, "/blog?blogId=") != nullptr) {
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blogId = atoi(blogIdStart + 7);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        // 查询博客详情
        sql_blog_tool tool;
        Blog blog = tool.select_blog_by_id(blogId);

        // 添加转义字符处理函数
        auto escapeJsonString = [](const string& input) {
            string output;
            for (char c : input) {
                switch (c) {
                    case '"': output += "\\\""; break;   // 双引号转义
                    case '\\': output += "\\\\"; break;  // 反斜杠转义
                    case '\b': output += "\\b"; break;   // 退格符
                    case '\f': output += "\\f"; break;   // 换页符
                    case '\n': output += "\\n"; break;   // 换行符
                    case '\r': output += "\\r"; break;   // 回车符
                    case '\t': output += "\\t"; break;   // 制表符
                    default:
                        // 处理其他不可打印字符
                        if (iscntrl(c)) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            output += buf;
                        } else {
                            output += c;
                        }
                }
            }
            return output;
        };

        // 处理标题和内容中的特殊字符
        string content = blog.get_blog_content();
        string escapedTitle = escapeJsonString(blog.get_blog_title());
        string escapedContent = escapeJsonString(content);
        
        // 构建 JSON 对象
        jsonData += "{";
        jsonData += "\"blogId\": " + std::to_string(blog.get_blog_id()) + ",";
        jsonData += "\"title\": \"" + escapedTitle + "\",";
        jsonData += "\"content\": \"" + escapedContent + "\",";
        jsonData += "\"userId\": " + std::to_string(blog.get_user_id()) + ",";
        jsonData += "\"postTime\": \"" + blog.get_blog_postTime() + "\"";
        jsonData += "}";

        return BLOG_DETAIL;
    }
    // 请求博客详情的同时也返回该博客对应的用户信息
    else if (strstr(m_url, "/user?blogId=") != nullptr) {
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blogId = atoi(blogIdStart + 7);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        // 获取用户id
        sql_blog_tool tool;
        int userid = tool.get_userid_by_blogid(blogId);

        // 根据用户id获取用户信息
        User user = tool.get_userdata_by_userid(userid);

        // 构建 JSON 对象
        jsonData += "{";
        jsonData += "\"avatar\": \"" + user.get_avatar() + "\",";
        jsonData += "\"username\": \"" + user.get_username() + "\",";
        jsonData += "\"article_count\": \"" + to_string(user.get_article_count()) + "\"";
        jsonData += "}";

        return BLOG_DETAIL;
    }
    // 获取博客详情对应的评论内容
    else if (strstr(m_url, "/get_blog_comments?blogId=") != nullptr) {
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blogId = atoi(blogIdStart + 7);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        // 添加转义字符处理函数
        auto escapeJsonString = [](const string& input) {
            string output;
            for (char c : input) {
                switch (c) {
                    case '"': output += "\\\""; break;   // 双引号转义
                    case '\\': output += "\\\\"; break;  // 反斜杠转义
                    case '\b': output += "\\b"; break;   // 退格符
                    case '\f': output += "\\f"; break;   // 换页符
                    case '\n': output += "\\n"; break;   // 换行符
                    case '\r': output += "\\r"; break;   // 回车符
                    case '\t': output += "\\t"; break;   // 制表符
                    default:
                        // 处理其他不可打印字符
                        if (iscntrl(c)) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            output += buf;
                        } else {
                            output += c;
                        }
                }
            }
            return output;
        };

        sql_blog_tool tool;
        vector<Comments> comments;
        comments = tool.get_comments_by_blogid(blogId);

        // 处理评论
        jsonData = "{";
        jsonData += "\"comments\": [";
        for (int i = 0; i < comments.size(); ++i) {
            Comments comment = comments[i];
            string escapedContent = escapeJsonString(comment.get_content());

            jsonData += "{";
            jsonData += "\"username\": \"" + comment.get_username() + "\",";
            jsonData += "\"content\": \"" + escapedContent + "\",";
            jsonData += "\"comment_time\": \"" + comment.get_comment_time() + "\"";
            jsonData += "}";

            if (i != comments.size() - 1) {
                jsonData += ",";
            }
        }
        jsonData += "]";
        jsonData += "}";

        return BLOG_DETAIL;
    }
    // 发表评论
    else if (strstr(m_url, "/add_comment") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id))
             return BAD_REQUEST;

        string post_body = url_decode(m_string);
        auto post_data = parse_post_data(post_body);
        
        string blog_id = post_data["blogId"];
        string content = post_data["content"];

        // 添加转义字符处理函数
        auto escapeJsonString = [](const string& input) {
            string output;
            for (char c : input) {
                switch (c) {
                    case '"': output += "\\\""; break;   // 双引号转义
                    case '\\': output += "\\\\"; break;  // 反斜杠转义
                    case '\b': output += "\\b"; break;   // 退格符
                    case '\f': output += "\\f"; break;   // 换页符
                    case '\n': output += "\\n"; break;   // 换行符
                    case '\r': output += "\\r"; break;   // 回车符
                    case '\t': output += "\\t"; break;   // 制表符
                    default:
                        // 处理其他不可打印字符
                        if (iscntrl(c)) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            output += buf;
                        } else {
                            output += c;
                        }
                }
            }
            return output;
        };

        string m_content = escapeJsonString(content);
        // 获取系统时间
        time_t now = time(nullptr);
        tm* local_time = localtime(&now);
        char buffer[100];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
        string postTime = string(buffer);

        // 将评论插入评论表
        Comments comment;
        comment.set_username(username);
        comment.set_blog_id(stoi(blog_id));
        comment.set_content(content);
        comment.set_comment_time(postTime);

        sql_blog_tool tool;
        tool.add_comment_by_blogid(comment);

        return REDIRECT_HOME;
    }
    // 返回当前登录的用户信息
    else if(strstr(m_url, "/get_current_user") != nullptr){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        // 需要验证会话是否存在
        if(cookie.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 根据用户名获取用户id
            int userid = tool.get_userid(username);

            User user;
            // 根据用户id获取用户信息
            user = tool.get_userdata_by_userid(userid);

            // 构建JSON响应
            jsonData += "{";
            jsonData += "\"avatar\": \"" + user.get_avatar() + "\",";
            jsonData += "\"username\": \"" + user.get_username() + "\",";
            jsonData += "\"article_count\": \"" + to_string(user.get_article_count()) + "\"";
            jsonData += "}";

            return BLOG_DETAIL;
        }
        return BAD_REQUEST;
    }
    // 这个用来修改博客
    else if (strstr(m_url, "/blog_editor_modify.html"))
    {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_login.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
	    }else{
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_editor_modify.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }
    }
    // 这个用来新建博客 处理需要验证的请求？
    else if (strstr(m_url, "/blog_editor.html"))    // 在申请页面时，判断是否登录，从而返回不同的页面
    {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_login.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
	    }else{
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_editor.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }
    }
    else if(strstr(m_url, "blog_home.html")){   
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_home.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
	    }else{
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_user_home.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }
    }
    else if(strstr(m_url, "blog_user_home.html")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            char *m_url_real = (char*)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/blog_user_home.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
            free(m_url_real);
        }else{
            return REDIRECT_HOME;
        }
    }
    else if (strstr(m_url, "/logout")){     // 注销并重定向到登录界面
        string username = cookie.getCookie("username");
        cookie.removeSession(username);
        return REDIRECT;
    }
    else if (strstr(m_url, "/comments"))
    {
        // 创建 JSON 格式的测试数据
        jsonData = R"([
            {
                "text": "这是第一条留言",
		        "timestamp": "2024-11-13 3:03:14"
            },
            {
                "text": "这是第二条留言",
		        "timestamp": "2024-11-13 16:03:14"
            }
        ])";
        
        // 将 JSON 数据加入响应
        return BLOG_DETAIL;
    }
    else if (strstr(m_url, "/message_board.html"))
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/message_board.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(strstr(m_url, "/user_center.html")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/user_center.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else{
            return REDIRECT_HOME;
        }
    }
    // 个人中心-个人资料
    else if (strstr(m_url, "/profile")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 根据用户名获取用户id
            int userid = tool.get_userid(username);

            User user;
            // 根据用户id获取用户信息
            user = tool.get_userdata_by_userid(userid);

            // 构建JSON响应
            jsonData += "{";
            jsonData += "\"username\": \"" + user.get_username() + "\",";
            jsonData += "\"email\": \"" + user.get_eamil() + "\",";
            jsonData += "\"description\": \"" + user.get_description() + "\"";
            jsonData += "}";

            return BLOG_DETAIL;
        }else{
            return BAD_REQUEST;
        }
    }
    // 个人中心-博客管理
    else if (strstr(m_url, "/manage")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 根据用户名获取用户id
            int userid = tool.get_userid(username);

            User user;
            // 根据用户id获取该用户的全部博客
            vector<Blog> blogs;
            blogs = tool.get_blogs_by_userid(userid);

            // 添加转义字符处理函数
        auto escapeJsonString = [](const string& input) {
            string output;
            for (char c : input) {
                switch (c) {
                    case '"': output += "\\\""; break;   // 双引号转义
                    case '\\': output += "\\\\"; break;  // 反斜杠转义
                    case '\b': output += "\\b"; break;   // 退格符
                    case '\f': output += "\\f"; break;   // 换页符
                    case '\n': output += "\\n"; break;   // 换行符
                    case '\r': output += "\\r"; break;   // 回车符
                    case '\t': output += "\\t"; break;   // 制表符
                    default:
                        // 处理其他不可打印字符
                        if (iscntrl(c)) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            output += buf;
                        } else {
                            output += c;
                        }
                }
            }
            return output;
        };

        // 构造 JSON 响应
        jsonData = "{";
        jsonData += "\"blogs\": [";
        for (int i = 0; i < blogs.size(); i++) {
            Blog blog = blogs[i];
            string escapedTitle = escapeJsonString(blog.get_blog_title());
            string escapedContent = escapeJsonString(blog.get_blog_content());
            jsonData += "{";
            jsonData += "\"blogId\": " + std::to_string(blog.get_blog_id()) + ",";
            jsonData += "\"title\": \"" + escapedTitle + "\",";
            jsonData += "\"postTime\": \"" + blog.get_blog_postTime() + "\"";
            jsonData += "}";
            if (i < blogs.size() - 1) jsonData += ",";
        }
        jsonData += "]";
        jsonData += "}";

        return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // 文件检查与映射
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;     // 插眼

    int fd = open(m_real_file, O_RDONLY);   // 以只读模式打开文件，返回文件描述符fd
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);   //通过mmap将文件的内容映射到进程的地址空间中
    close(fd);
    return FILE_REQUEST;
}

// 解除内存映射
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);    //解除通过mmap（）创建的内存映射，将文件内容从进程的地址空间删除
        m_file_address = 0;
    }
}

// 将响应数据写入客户端
bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0) //没有待发送的数据
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);  //将m_iv指向的缓冲区的数据写入m_sockfd

        if (temp < 0)   //writev的返回值小于0，表示写操作失败
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        // 若已经发送完第一个缓冲区
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
            jsonData.clear();   // 发送完毕，清空jsonData

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

// 向HTTP响应中添加格式化的字符串
bool http_conn::add_response(const char *format, ...)
{
    // 若缓冲区索引大于缓冲区大小，说明缓冲区已满
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    va_list arg_list;
    va_start(arg_list, format); // 初始化参数列表
    // 将格式化字符串输出到m_write_buf数组中的m_write_buf + m_write_idx位置
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);   // 释放参数列表
        return false;
    }

    // 修改缓冲区索引
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("request:%s", m_write_buf);    // 写入日志

    return true;
}

// 设置响应状态行
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 设置响应头部
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}

// 设置内容长度头
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

// 设置内容类型头
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 设置连接管理方式
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 添加空行，表示头部和主体之间的分隔
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

// 添加实际的响应内容，将内容体添加到响应中
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

// 根据请求状态生成相应的HTTP响应
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case REDIRECT:
    {
        add_status_line(302, ok_302_title);
        add_response("Location: /blog_login.html\r\n");
        string cookie_header = cookie.generateCookieHeaders();
        add_response(cookie_header.c_str());
        add_headers(strlen(ok_302_title));
    }
    case REDIRECT_HOME:
    {
        add_status_line(302, ok_302_title);
        add_response("Location: /blog_home.html\r\n");
        add_headers(strlen(ok_302_title));
    }
    case REDIRECT_USER_HOME:
    {
        add_status_line(302, ok_302_title);
        add_response("Location: /blog_user_home.html\r\n");
        add_headers(strlen(ok_302_title));
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)   // 若文件不为空
        {
            // 添加cookie响应头  
            // string cookie_headers = cookie.generateCookieHeaders();
            // if(!cookie_headers.empty()){
            //     add_response(cookie_headers.c_str());
            // }  
            add_headers(m_file_stat.st_size);       
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    case LOGIN_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)   // 若文件不为空
        {
            // 添加cookie响应头  
            string cookie_headers = cookie.generateCookieHeaders();
            if(!cookie_headers.empty()){
                add_response(cookie_headers.c_str());
            }  
            add_headers(m_file_stat.st_size);       
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    case BLOG_DATA:         // 构造返回博客的http响应
    {
        add_status_line(200, ok_200_title);
        // add_headers(strlen(m_write_buf));
        add_response("Content-Type: application/json\r\n"); // 因为是JSON字符串
        add_headers(jsonData.length());

        // add_content(jsonData.c_str());
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = (void*)jsonData.c_str();
        m_iv[1].iov_len = jsonData.size();
        
        m_iv_count = 2;
        bytes_to_send = m_write_idx + jsonData.length();
        return true;
    }
    case BLOG_DETAIL:         // 返回博客的详细数据
    {
        add_status_line(200, ok_200_title);
        // add_headers(strlen(m_write_buf));
        add_response("Content-Type: application/json\r\n"); // 因为是JSON字符串
        add_headers(jsonData.length());
        
        // add_content(jsonData.c_str());
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = (void*)jsonData.c_str();
        m_iv[1].iov_len = jsonData.size();
        
        m_iv_count = 2;
        bytes_to_send = m_write_idx + jsonData.length();
        return true;
    }
    case BLOG_USER_HOME:
    {
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;

        m_iv_count = 1;
        bytes_to_send = m_write_idx;
        return true;
    }
    case OK:
    {
        add_status_line(200, ok_200_title);
        add_response("Content-Type: application/json\r\n"); // 因为是JSON字符串
        add_headers(strlen(ok_200_title));

    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
    HTTP_CODE read_ret = process_read();    // 标识客户端请求的处理结果
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);   // 标识响应是否成功生成
    if (!write_ret) // 若写失败，关闭连接
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}
