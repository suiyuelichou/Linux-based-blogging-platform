#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <string>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <regex>
#include <bcrypt/BCrypt.hpp>
#include <unordered_map>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *ok_302_title = "Found";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_401_title = "Unauthorized";
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

    mysql_free_result(result);
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
    memset(&event, 0, sizeof(event));
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
    memset(&event, 0, sizeof(event)); // 新增初始化
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
    // 如果当前是在处理请求体，直接返回LINE_OK
    if(m_check_state == CHECK_STATE_CONTENT){
        return LINE_OK;
    }

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
        // m_read_buf + m_read_idx ：计算出缓冲区中实际写入数据的位置。在这里获取fd的数据
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

    // 检测管理员路径
    if (strstr(m_url, "/admins") != nullptr) {
        is_admin_request = true;  // 标记为管理员请求
        // LOG_INFO("Admin request detected: %s", m_url);
    }

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
            m_content_start = m_checked_idx;
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
    else if (strncasecmp(text, "Content-Type:", 13) == 0) // 提取 Content-Type
    {
        text += 13;
        text += strspn(text, " \t");
        m_content_type = text; // 保存 Content-Type

        // 检查 boundary 参数
        const char* boundary_key = "boundary=";
        char* boundary_pos = strstr(text, boundary_key);
        if (boundary_pos != nullptr)
        {
            boundary_pos += strlen(boundary_key);
            
            // 移除可能的引号
            if (*boundary_pos == '"')
            {
                boundary_pos++;
                char* quote_end = strchr(boundary_pos, '"');
                if (quote_end != nullptr)
                {
                    m_boundary = string(boundary_pos, quote_end - boundary_pos);
                    // return NO_REQUEST;
                }
            }
            
            // 处理无引号情况
            char* end = strpbrk(boundary_pos, " ;\r\n");
            if (end != nullptr)
            {
                m_boundary = string(boundary_pos, end - boundary_pos);
            }
            else
            {
                m_boundary = boundary_pos; // 使用整个剩余字符串
            }
        }
    }
    else if(strncasecmp(text, "Cookie:", 7) == 0){
        text += 7;
        text += strspn(text, " \t");
        if(is_admin_request){
            cookie_admin.parseCookieHeader(text);
        }else{
            cookie.parseCookieHeader(text);     // 解析并存储cookie字符串
        }
    }
    else    // 若某一行的请求头不属于上述之一，则会重复写入日志，并在前面添加报警
    {
        // LOG_INFO("oop!unknow header: %s", text);
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

// 解析http请求体内容并存储 这是表单类型的数据的（经过部分修改，也能解析简单的json）
unordered_map<string, string> http_conn::parse_post_data(const string& body) {
    unordered_map<string, string> post_data;
    
    // Trim whitespace
    auto trim = [](const string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        size_t last = str.find_last_not_of(" \t\n\r");
        return first != string::npos ? str.substr(first, last - first + 1) : "";
    };

    // Check if it's a JSON object
    if (!body.empty() && body[0] == '{' && body.back() == '}') {
        string key, value;
        bool inKey = true, inString = false, escaping = false;
        int bracketCount = 0, bracketLevel = 0;

        for (size_t i = 1; i < body.size() - 1; ++i) {
            char c = body[i];

            // Handle escaping
            if (escaping) {
                if (inKey) key += c;
                else value += c;
                escaping = false;
                continue;
            }

            // Escape character
            if (c == '\\') {
                escaping = true;
                if (inKey) key += c;
                else value += c;
                continue;
            }

            // Handle nested objects and arrays
            if (c == '{' || c == '[') {
                bracketCount++;
                if (inKey) key += c;
                else value += c;
                bracketLevel++;
                continue;
            }

            if (c == '}' || c == ']') {
                bracketCount--;
                if (inKey) key += c;
                else value += c;
                bracketLevel--;
                continue;
            }

            // String handling
            if (c == '"') {
                if (!inString) {
                    // Start of a string
                    inString = true;
                }
                else {
                    // End of a string
                    if (bracketLevel == 0) {
                        if (inKey) {
                            // Finished reading key
                            inKey = false;
                        }
                        else {
                            // Finished reading value
                            post_data[trim(key)] = trim(value);
                            key.clear();
                            value.clear();
                            inKey = true;
                        }
                    }
                    inString = false;
                }
                continue;
            }

            // Add characters if inside a string or key/value
            if (inString || bracketLevel > 0) {
                if (inKey) key += c;
                else value += c;
                continue;
            }

            // Normal parsing
            if (!inString) {
                if (c == ':' && bracketLevel == 0) {
                    inKey = false;
                }
                else if (c == ',' && bracketLevel == 0) {
                    inKey = true;
                }
            }
        }
    }
    // Fallback to form-urlencoded parsing
    else {
        size_t start = 0, pos;
        while ((pos = body.find('&', start)) != string::npos) {
            string pair = body.substr(start, pos - start);
            size_t equalPos = pair.find('=');
            if (equalPos != string::npos) {
                string key = pair.substr(0, equalPos);
                string value = pair.substr(equalPos + 1);
                post_data[key] = value;
            }
            start = pos + 1;
        }

        // Process last pair
        string lastPair = body.substr(start);
        size_t lastEqualPos = lastPair.find('=');
        if (lastEqualPos != string::npos) {
            string key = lastPair.substr(0, lastEqualPos);
            string value = lastPair.substr(lastEqualPos + 1);
            post_data[key] = value;
        }
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

// 用于解析 multipart/form-data（头像） 并保存文件
string http_conn::handle_file_upload(const string& boundary, const string& body, const string& upload_dir)
{
    // 确保boundary格式正确（协议要求以--开头）
    string delimiter = "--" + boundary;

    // 查找第一个有效part的起始位置
    size_t part_start = body.find(delimiter);
    if (part_start == string::npos) return "";
    part_start += delimiter.length();

    // 跳过头部，定位到文件名的位置
    size_t name_pos = body.find("filename=\"", part_start);
    if (name_pos == string::npos) return "";
    name_pos += 10; // 跳过"filename=\""
    size_t name_end = body.find('\"', name_pos);
    string filename = body.substr(name_pos, name_end - name_pos);
    filename = sanitize_filename(filename);

    // 定位文件内容起始位置（跳过头部后的空行）
    size_t header_end = body.find("\r\n\r\n", name_end);
    if (header_end == string::npos) return "";
    size_t file_start = header_end + 4;

    // 查找当前part的结束位置（下一个boundary前）
    size_t file_end = body.find(delimiter, file_start);
    if (file_end == string::npos) {
        // 可能是最后一个part，尝试查找结束符
        file_end = body.find("--" + boundary + "--", file_start);
        if (file_end == string::npos) return "";
    }
    // 回退到内容结束位置（减去前置\r\n）
    file_end -= 2;

    // 确保数据范围有效
    if (file_end <= file_start) return "";

    // 写入文件（二进制模式）
    string file_path = upload_dir + "/" + generate_unique_filename(filename);
    ofstream out(file_path, ios::binary);
    if (!out) return "";
    out.write(&body[file_start], file_end - file_start); // 直接操作string底层数据

    return file_path;
}

// 解析multipart/form-data格式的请求体
bool http_conn::parse_multipart_form_data(const std::string &boundary, 
                                        std::map<std::string, std::string> &form_data,
                                        std::map<std::string, file_data_t> &files)
{
    // 清理之前可能存在的数据
    form_data.clear();
    for (auto& file : files) {
        free(file.second.data);
    }
    files.clear();

    // 构造完整的boundary字符串（确保格式正确）
    std::string delimiter = "--" + boundary;
    std::string end_boundary = delimiter + "--";
    
    // 将读取缓冲区转换为string以使用更高效的string操作
    std::string body(m_read_buf + m_content_start, m_read_idx - m_content_start);
    
    // 验证内容是否为空
    if (body.empty()) {
        return false;
    }
    
    size_t pos = 0;
    while (pos < body.length()) {
        // 查找boundary
        size_t part_start = body.find(delimiter, pos);
        if (part_start == std::string::npos) break;
        
        // 检查是否是结束标记
        if (body.substr(part_start, end_boundary.length()) == end_boundary) {
            break;
        }
        
        // 跳过boundary
        part_start += delimiter.length();
        
        // 确保在有效范围内
        if (part_start >= body.length()) break;
        
        // 跳过可能的换行
        if (body[part_start] == '\r' && part_start + 1 < body.length() && body[part_start + 1] == '\n') {
            part_start += 2;
        }
        
        // 查找头部结束位置
        size_t header_end = body.find("\r\n\r\n", part_start);
        if (header_end == std::string::npos) break;
        
        // 解析头部字段
        std::string headers = body.substr(part_start, header_end - part_start);
        std::string name;
        std::string filename;
        bool is_file = false;
        
        // 查找 Content-Disposition 行
        size_t content_disp_pos = headers.find("Content-Disposition:");
        if (content_disp_pos != std::string::npos) {
            // 解析 name 参数 - 直接使用string的查找函数
            size_t name_pos = headers.find("name=\"", content_disp_pos);
            if (name_pos != std::string::npos) {
                name_pos += 6; // 跳过 name="
                size_t name_end = headers.find("\"", name_pos);
                if (name_end != std::string::npos) {
                    name = headers.substr(name_pos, name_end - name_pos);
                    LOG_INFO("找到字段名: %s", name.c_str());
                }
            }

            // 解析 filename 参数
            size_t filename_pos = headers.find("filename=\"");
            if (filename_pos != std::string::npos) {
                filename_pos += 10; // 跳过 filename="
                size_t filename_end = headers.find("\"", filename_pos);
                if (filename_end != std::string::npos) {
                    filename = headers.substr(filename_pos, filename_end - filename_pos);
                    is_file = true;
                    LOG_INFO("找到文件字段: %s, 文件名: %s", name.c_str(), filename.c_str());
                }
            }
        }

        // 如果没有找到有效的name，跳过这部分
        if (name.empty()) {
            pos = header_end + 4;
            continue;
        }
        
        // 移动到内容开始位置
        size_t content_start = header_end + 4;
        
        // 查找下一个boundary或结束boundary
        size_t next_part = body.find(delimiter, content_start);
        if (next_part == std::string::npos) break;
        
        // 计算内容长度（减去尾部的\r\n）
        size_t content_length = next_part - content_start;
        if (content_length >= 2 && body[next_part - 2] == '\r' && body[next_part - 1] == '\n') {
            content_length -= 2;
        }
        
        // 处理文件或表单数据
        if (is_file && !filename.empty()) {
            file_data_t file_data;
            file_data.filename = filename;
            file_data.size = content_length;
            
            // 分配内存并复制数据
            file_data.data = (char *)malloc(content_length);
            if (!file_data.data) {
                // 内存分配失败，清理已分配的资源
                for (auto& file : files) {
                    free(file.second.data);
                }
                files.clear();
                return false;
            }
            
            memcpy(file_data.data, &body[content_start], content_length);
            files[name] = file_data;
            
            LOG_INFO("处理文件字段: %s, 文件名: %s, 大小: %zu 字节", 
                     name.c_str(), filename.c_str(), content_length);
        } else {
            // 处理普通表单字段 - 直接使用string子串
            form_data[name] = body.substr(content_start, content_length);
            LOG_INFO("处理表单字段: %s = %s", 
                     name.c_str(), form_data[name].c_str());
        }
        
        // 移动到下一个boundary位置
        pos = next_part;
    }
    
    LOG_INFO("解析完成: %zu 个表单字段, %zu 个文件", 
             form_data.size(), files.size());
    
    return !form_data.empty() || !files.empty();
}


// 获取文件扩展名
string http_conn::get_file_extension(const std::string &filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return filename.substr(dot_pos + 1);
    }
    return "";
}

// 移除文件名中的潜在危险字符
string http_conn::sanitize_filename(const std::string &filename)
{
    string safe_filename = filename;

    // 移除非法字符，例如 "..", "/" 等
    safe_filename.erase(remove_if(safe_filename.begin(), safe_filename.end(), [](char c) {
        return !(isalnum(c) || c == '.' || c == '_' || c == '-');
    }), safe_filename.end());

    // 防止空文件名
    if (safe_filename.empty()) {
        safe_filename = "default_file";
    }

    return safe_filename;
}

// 生成一个唯一的文件名，避免文件名冲突
string http_conn::generate_unique_filename(const std::string &filename)
{
     // 获取当前时间戳（毫秒级别）
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // 提取文件扩展名
    size_t dot_pos = filename.find_last_of('.');
    std::string base_name = (dot_pos != std::string::npos) ? filename.substr(0, dot_pos) : filename;
    std::string extension = (dot_pos != std::string::npos) ? filename.substr(dot_pos) : "";

    // 生成唯一文件名
    return base_name + "_" + std::to_string(milliseconds) + extension;
}

// 检测注册的用户名是否合法
bool http_conn::is_valid_username(const char *name)
{
    int len = strlen(name);
    if (len < 6 || len > 20) return false;

    for (int i = 0; i < len; ++i) {
        if (!isalnum(name[i])) return false; // 只能是字母或数字
    }
    return true;
}

// 检测注册的密码是否合法
bool http_conn::is_valid_password(const char *password)
{
    int len = strlen(password);
    if (len < 8 || len > 20) return false; // 修正长度限制

    bool has_letter = false, has_digit = false, has_symbol = false;
    for (int i = 0; i < len; ++i) {
        if (isalpha(password[i])) has_letter = true;
        else if (isdigit(password[i])) has_digit = true;
        else if (ispunct(password[i])) has_symbol = true; // 仅检测符号
    }

    return has_letter && has_digit && has_symbol; // 需包含三种类型
}

// 检测邮箱格式是否合法
bool http_conn::is_valid_email(const char *email)
{
    if (!email) return false;
    size_t len = strlen(email);
    if (len < 5) return false; // 最短邮箱格式 "a@b.c"

    string email_str(email);
    size_t at_pos = email_str.find('@');
    size_t dot_pos = email_str.find('.', at_pos);

    // 确保 '@' 和 '.' 存在，并且位置合法
    if (at_pos == string::npos || dot_pos == string::npos) return false;
    if (at_pos == 0 || at_pos == len - 1) return false; // 不能是 "@" 开头或结尾
    if (dot_pos == at_pos + 1) return false; // 不能是 "user@.com"
    if (dot_pos == len - 1) return false; // 不能是 "user@example."

    return true;
}

// 检测头像格式是否合法
bool http_conn::is_valid_avatar(const char *avatar)
{
    if (!avatar) return false;

    // 检查是否是默认头像路径之一
    string avatar_path(avatar);
    vector<string> valid_paths = {
        "img/default_touxiang.jpg",
        "img/avatar1.jpg",
        "img/avatar2.jpg", 
        "img/avatar3.jpg"
    };

    for (const auto& path : valid_paths) {
        if (avatar_path == path) {
            return true;
        }
    }

    return false;
}

// 辅助函数：解析单个ID
void http_conn::parse_id(char *start, size_t length, vector<int> &ids)
{
    char num_buf[32] = {0};
    size_t num_len = 0;
    
    // 提取有效数字字符
    for (size_t i = 0; i < length && num_len < sizeof(num_buf)-1; ++i) {
        if (isdigit(start[i])) {
            num_buf[num_len++] = start[i];
        }
    }
    
    if (num_len > 0) {
        int id = atoi(num_buf);
        if (id > 0) ids.push_back(id);
    }
}

// 添加转义字符处理函数
string escapeJsonString(const string& input) {
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
        // text = get_line();  // 获取当前行内容，并将指针赋值给text
        // m_start_line = m_checked_idx;
        // LOG_INFO("%s", text);   // 将当前行写入日志

        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:   // 转去解析请求行
            {
                text = get_line();  // 获取当前行内容，并将指针赋值给text
                m_start_line = m_checked_idx;
                LOG_INFO("%s", text);

                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:    // 转去解析请求头
            {
                text = get_line();  // 获取当前行内容，并将指针赋值给text
                m_start_line = m_checked_idx;
                LOG_INFO("%s", text);

                ret = parse_headers(text);  // 空行在这个函数里面判断了
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST)
                {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT:   // 转去解析请求体 这边要判断请求体是否完整
            {
                // text = get_line();  // 获取当前行内容，并将指针赋值给text
                // m_start_line = m_checked_idx;
                // LOG_INFO("%s", text);

                // ret = parse_content(text);
                // if (ret == GET_REQUEST)
                //     return do_request();
                // line_status = LINE_OPEN;
                // break;
                // 检查是否读取到了完整的请求体
                if(m_read_idx - m_content_start >= m_content_length){
                    text = m_read_buf + m_content_start;
                    LOG_INFO("%s", text);
                    text[m_content_length] = '\0';
                    m_string = text;
                    m_post_content.assign(text, m_content_length);  // 直接按长度拷贝，避免了'\0'导致数据截断
                    return do_request();
                }

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
    // if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    // {

    //     //根据标志判断是登录检测还是注册检测
    //     char flag = m_url[1];

    //     char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //     strcpy(m_url_real, "/");
    //     strcat(m_url_real, m_url + 2);
    //     strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
    //     free(m_url_real);

    //     //将用户名和密码提取出来
    //     //user=123&passwd=123
    //     int i;
    //     for (i = 5; m_string[i] != '&'; ++i)    //？m_string存储的是请求头哪部分的数据
    //         name[i - 5] = m_string[i];
    //     name[i - 5] = '\0';

    //     int j = 0;
    //     for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
    //         password[j] = m_string[i];
    //     password[j] = '\0';

    //     // 如果是注册，先检测数据库中是否有重名的
    //     // 修改：在这里进行用户名和密码的后端检测
    //     if (*(p + 1) == '3')
    //     {
    //         // 校验用户名
    //         if (!is_valid_username(name))
    //         {
    //             strcpy(m_url, "/blog_registerError.html"); // 用户名不符合要求
    //         }

    //         // 校验密码
    //         if (!is_valid_password(password))
    //         {
    //             strcpy(m_url, "/blog_registerError.html"); // 密码不符合要求
    //         }

    //         // 生成 bcrypt 哈希
    //         std::string hashed_password = BCrypt::generateHash(password);

    //         // 没有重名的，进行增加数据
    //         char *sql_insert = (char *)malloc(sizeof(char) * 200);
    //         snprintf(sql_insert, 300, "INSERT INTO user(username, password) VALUES('%s', '%s')", name, hashed_password.c_str());

    //         if (users.find(name) == users.end()) // 哈希表中不存在相同的用户名
    //         {
    //             m_lock.lock();
    //             int res = mysql_query(mysql, sql_insert); // 执行sql_insert中的语句
    //             if (!res)
    //                 users.insert(pair<string, string>(name, hashed_password)); // 数据插入成功后，存入users中
    //             m_lock.unlock();

    //             if (!res)
    //                 strcpy(m_url, "/blog_login.html");
    //             else
    //                 strcpy(m_url, "/blog_registerError.html");
    //         }
    //         else
    //             strcpy(m_url, "/blog_registerError.html");

    //         free(sql_insert); // 释放分配的内存
    //     }
    //     else if (*(p + 1) == '2')   //如果是登录，直接判断
    //     {
    //         //若浏览器端输入的用户名存在
    //         if (users.find(name) != users.end()){
    //             // 数据库中存储的哈希密码
    //             std::string stored_hash = users[name];
    //             // 验证密码   这里后面要把users[name] == password（方便测试）删掉，只保留哈希的验证
    //             if (BCrypt::validatePassword(password, stored_hash) || users[name] == password){
    //                 // 创建新会话
    //                 // cookie.createSession(name);
    //                 char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //                 try{
    //                     cookie.createSession(name);

    //                     sql_blog_tool tool;
    //                     int userid = tool.get_userid(name);
    //                     tool.update_last_login_time(userid);

    //                     current_username = name;
    //                     islogin = true;
    //                     strcpy(m_url, "/blog_home.html");
    //                     strcpy(m_url_real, "/blog_home.html");

    //                     // 测试
    //                     // // 查看所有普通登录用户
    //                     // vector<string> logged_users = Cookie::getActiveUsers();
    //                     // cout << "Logged Users: ";
    //                     // for (const auto& user : logged_users) {
    //                     //     cout << user << " ";
    //                     // }

    //                     // 查看详细会话信息（含session_id和最后活跃时间）
    //                     auto all_sessions = Cookie::getAllSessions();
    //                     for (const auto& entry : all_sessions) {
    //                         const string& user = entry.first;
    //                         const pair<string, time_t>& session = entry.second;
    //                         cout << "User: " << user 
    //                             << " | Session ID: " << session.first
    //                             << " | Last Active: " << session.second << endl;
    //                     }


    //                 } catch(const runtime_error& e){    // 若当前用户已登录，再登录会返回错误页面(用户已登录)
    //                     strcpy(m_url, "/blog_loginErrorExit.html");
    //                     strcpy(m_url_real, "/blog_loginErrorExit.html");
    //                 }

    //                 strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    //                 free(m_url_real);
    //             }else{
    //                 strcpy(m_url, "/blog_loginError.html");
    //                 char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //                 strcpy(m_url_real, "/blog_loginError.html");
    //                 strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    //                 free(m_url_real);
    //             }
    //         }
    //         else{
    //             strcpy(m_url, "/blog_loginError.html");
    //             char *m_url_real = (char *)malloc(sizeof(char) * 200);
    //             strcpy(m_url_real, "/blog_loginError.html");
    //             strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
    //             free(m_url_real);
    //         }
            
    //         // 文件检查与映射
    //         if (stat(m_real_file, &m_file_stat) < 0)
    //             return NO_RESOURCE;

    //         if (!(m_file_stat.st_mode & S_IROTH))
    //             return FORBIDDEN_REQUEST;

    //         if (S_ISDIR(m_file_stat.st_mode))
    //             return BAD_REQUEST;     // 插眼

    //         int fd = open(m_real_file, O_RDONLY);   // 以只读模式打开文件，返回文件描述符fd
    //         m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);   //通过mmap将文件的内容映射到进程的地址空间中
    //         close(fd);
    //         return LOGIN_REQUEST;
    //     }

    // }

    // 检查当前用户名是否已经被注册
    if(strstr(m_url, "/check_username_is_exist?username=") != nullptr){
        // 解析 username
        const char* usernameStart = strstr(m_url, "username=");
        if (usernameStart == nullptr) {
            return BAD_REQUEST;
        }

        // 跳过 "username=" 的长度
        string username = usernameStart + 9;

        // 检查用户名是否为空
        if (username.empty()) {
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        if(tool.check_username_is_exist(username)){
            jsonData = "{\"exists\": true}";
        } else {
            jsonData = "{\"exists\": false}";
        }
        return BLOG_DATA;
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

            
            if(tool.insert_blog(blog)){
                tool.increase_article_count(userid);
            }else{
                return BAD_REQUEST;
            }

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
                if(tool.delete_blog_by_blogid(blogId)){
                    tool.decrease_article_count(userid);
                }
                return REDIRECT_USER_HOME;
            }else{
                return BAD_REQUEST;
            }
        }
    }
    // 这里用来判断是否修改用户密码
    // else if(m_method == PATCH && strstr(m_url, "/update_password") != nullptr){
    //     string username = cookie.getCookie("username");
    //     string session_id = cookie.getCookie("session_id");

    //     if(cookie.validateSession(username, session_id)){
    //         // 获取userId
    //         sql_blog_tool tool;
    //         int userid = tool.get_userid(username);

    //         string body = url_decode(m_string);
    //         auto post_data = parse_post_data(body);

    //         string old_password = post_data["oldPassword"];
    //         string new_password = post_data["newPassword"];

    //         // 这里是用来检测用户输入的旧密码对不对
    //         if(users[username] == old_password){
    //             tool.modify_password_by_username(username, new_password);
    //             users[username] = new_password;
    //             return REDIRECT_USER_HOME;
    //         }else{
    //             return BAD_REQUEST;
    //         }
    //     }
    // }

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
    // 登录
    else if (strstr(m_url, "/api/user/login")) {
        json requestJson = json::parse(m_string);
        string username = requestJson["username"];
        string password = requestJson["password"];

        // 检查用户名是否存在
        if(users.find(username) != users.end()){
            string stored_hash = users[username];
            if(BCrypt::validatePassword(password, stored_hash) || password == stored_hash){
                // 创建新会话
                try{
                    cookie.createSession(username);
                    sql_blog_tool tool;
                    int userid = tool.get_userid(username);
                    tool.update_last_login_time(userid);
                    int view_count = tool.get_view_count_by_userid(userid);
                    int like_count = tool.get_blog_liked_count_by_userid(userid);
                    User user = tool.get_userdata_by_userid(userid);

                    json response = {
                        {"success", true},
                        {"message", "登录成功"},
                        {"user", {
                            {"username", username},
                            {"avatar", user.get_avatar()},
                            {"articleCount", user.get_article_count()},
                            {"viewCount", view_count},
                            {"likeCount", like_count}
                        }}
                    };

                    jsonData = response.dump();

                    return LOGIN;
                }catch(const runtime_error& e){
                    return BAD_REQUEST;
                }
            }else{
                json response = {
                    {"success", false},
                    {"message", "用户名或密码错误"}
                };
                jsonData = response.dump();
                return LOGIN;
            }
        }else{
            json response = {
                {"success", false},
                {"message", "用户名或密码错误"}
            };
            jsonData = response.dump();
            return LOGIN;
        }
    }
    // 注销
    else if (strstr(m_url, "/api/user/logout")) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            cookie.removeSession(username);
            json response = {
                {"success", true},
                {"message", "已成功退出登录"}
            };
            jsonData = response.dump();
            return LOGOUT;
        }else{
            json response = {
                {"success", false},
                {"message", "退出失败"}
            };
            jsonData = response.dump();
            return LOGOUT;
        }
    }
    // 注册
    else if (strstr(m_url, "/api/user/register")) {
        json requestJson = json::parse(m_string);
        string username = requestJson["username"];
        string password = requestJson["password"];
        string avatar = requestJson["avatar"];
        string email = requestJson["email"];

        if(!is_valid_username(username.c_str())){
            json response = {
                {"success", false},
                {"message", "用户名格式不正确"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
        if(!is_valid_password(password.c_str())){
            json response = {
                {"success", false},
                {"message", "密码格式不正确"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
        if(!is_valid_email(email.c_str())){
            json response = {
                {"success", false},
                {"message", "邮箱格式不正确"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
        if(!is_valid_avatar(avatar.c_str())){
            json response = {
                {"success", false},
                {"message", "头像格式不正确"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
        // 生成 bcrypt 哈希
        std::string hashed_password = BCrypt::generateHash(password);

        if (users.find(username) == users.end()) // 哈希表中不存在相同的用户名
        {
            m_lock.lock();
            sql_blog_tool tool;
            if(tool.user_register(username, hashed_password, email, avatar)){
                users.insert(pair<string, string>(username, hashed_password));
                m_lock.unlock();
                json response = {
                    {"success", true},
                    {"message", "注册成功"},
                    {"user", {
                        {"username", username},
                        {"avatar", avatar},
                        {"email", email}
                    }}
                };
                jsonData = response.dump();
                return BLOG_DATA;
            }else{
                json response = {
                    {"success", false},
                    {"message", "注册失败"}
                };
                jsonData = response.dump();
                return BLOG_DATA;
            }
        }else{
            json response = {
                {"success", false},
                {"message", "用户名已存在"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
    }
    // 获取所有博客
    else if (m_method == GET && strstr(m_url, "/api/articles")) {
        int page = 1, size = 6;
        char* pageParam = strstr(m_url, "page=");
        char* sizeParam = strstr(m_url, "size=");
        char* categoryParam = strstr(m_url, "category=");
        char* tagParam = strstr(m_url, "tag=");
        
        if (pageParam) {
            page = std::atoi(pageParam + 5);
            if (page <= 0) page = 1;
        }
        if (sizeParam) {
            size = std::atoi(sizeParam + 5);
            if (size <= 0 || size > 100) size = 6;
        }
        
        // 查询数据库
        sql_blog_tool tool;
        vector<Blog> blogs;
        int totalCount;
        
        if (categoryParam) {
            // 解析category参数值
            string category;
            char* end = strchr(categoryParam + 9, '&');
            if (end) {
                category = string(categoryParam + 9, end - (categoryParam + 9));
            } else {
                category = string(categoryParam + 9);
            }
            category = url_decode(category);
            
            // 按分类查询博客
            int categoryId = tool.get_category_id_by_name(category);
            if(categoryId == -1){
                return BAD_REQUEST;
            }
            blogs = tool.get_blogs_by_category_id(categoryId, page, size);
            totalCount = tool.get_total_blog_count_by_category(categoryId);
        } else if (tagParam) {
            // 解析tag参数值
            string tag;
            char* end = strchr(tagParam + 4, '&');
            if (end) {
                tag = string(tagParam + 4, end - (tagParam + 4));
            } else {
                tag = string(tagParam + 4);
            }
            tag = url_decode(tag);
            
            // 按标签查询博客
            int tagId = tool.get_tag_id_by_name(tag);
            if(tagId == -1){
                return BAD_REQUEST;
            }
            blogs = tool.get_blogs_by_tag_id(tagId, page, size);
            totalCount = tool.get_total_blog_count_by_tag(tagId);
        } else {
            // 不按分类查询
            blogs = tool.get_blogs_by_page(page, size);
            totalCount = tool.get_total_blog_count();
        }

        // 构建文章数组
        json response = {
            {"articles", nlohmann::json::array()}
        };
        for (auto& blog : blogs) {
            nlohmann::json article;
            
            // 获取相关数据
            User user = tool.get_userdata_by_userid(blog.get_user_id());
            std::string category = tool.get_cotegoriename_by_cotegorieid(blog.get_category_id());
            int likes = tool.get_blog_likes_count(blog.get_blog_id());
            int comments = tool.get_blog_comments_count(blog.get_blog_id());
            
            // 填充文章数据
            article["id"] = blog.get_blog_id();
            article["title"] = blog.get_blog_title();
            article["excerpt"] = blog.get_blog_content();
            article["thumbnail"] = blog.get_thumbnail(); // 之前代码里注释掉了 user.get_avatar()
            article["author"] = user.get_username();
            article["authorAvatar"] = user.get_avatar();
            article["date"] = blog.get_blog_postTime();
            article["category"] = category;
            article["views"] = blog.get_views();
            article["likes"] = likes;
            article["comments"] = comments;
            
            // 添加到文章数组
            response["articles"].push_back(article);
        }
        
        // 添加分页信息
        int totalPages = (totalCount + size - 1) / size; // 向上取整
        response["totalPages"] = totalPages;
        response["currentPage"] = page;
        response["totalArticles"] = totalCount;

        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 获取热门博客
    else if (strstr(m_url, "/api/popular")) {
        int size = 5;
        char* sizeParam = strstr(m_url, "size=");
        if (sizeParam) {
            size = std::atoi(sizeParam + 5);
            if (size <= 0 || size > 100) size = 5;
        }
        
        // 查询数据库
        sql_blog_tool tool;
        vector<Blog> blogs = tool.get_blogs_by_page_by_views(1, size);

        // 构造 JSON 响应
        jsonData = "{";
        jsonData += "\"articles\": [";
        for (int i = 0; i < blogs.size(); i++) {
            Blog blog = blogs[i];
            string escapedTitle = escapeJsonString(blog.get_blog_title());
            int likes = tool.get_blog_likes_count(blog.get_blog_id());
            jsonData += "{";
            jsonData += "\"id\": " + std::to_string(blog.get_blog_id()) + ",";
            jsonData += "\"title\": \"" + escapedTitle + "\",";
            jsonData += "\"thumbnail\": \"" + blog.get_thumbnail() + "\",";
            jsonData += "\"views\": " + std::to_string(blog.get_views()) + ",";
            jsonData += "\"likes\": " + std::to_string(likes);
            jsonData += "}";
            if (i < blogs.size() - 1) jsonData += ",";
        }
        jsonData += "]";
        jsonData += "}";

        return BLOG_DATA;
    }
    // 获取所有分类
    else if (strstr(m_url, "/api/categories")) {
        // 查询数据库
        sql_blog_tool tool;
        vector<Categories> categories = tool.get_categories();

        // 构造 JSON 响应
        jsonData = "{";
        jsonData += "\"categories\": [";
        for (int i = 0; i < categories.size(); i++) {
            Categories category = categories[i];
            string escapedName = escapeJsonString(category.get_name());
            int count = tool.get_total_blog_count_by_category(category.get_id());
            jsonData += "{";
            jsonData += "\"name\": \"" + escapedName + "\",";
            jsonData += "\"count\": \"" + to_string(count) + "\"";
            jsonData += "}";
            if (i < categories.size() - 1) jsonData += ",";
        }
        jsonData += "]";
        jsonData += "}";

        return BLOG_DATA;
    }
    // 获取用户信息
    else if (strstr(m_url, "/api/user/info")) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            sql_blog_tool tool;
            int userid = tool.get_userid(username);
            User user = tool.get_userdata_by_userid(userid);
            int view_count = tool.get_view_count_by_userid(userid);
            int like_count = tool.get_blog_liked_count_by_userid(userid);
            int article_count = tool.get_article_count_by_userid(userid);
            int comment_count = tool.get_blog_comments_count_by_userid(username);

            jsonData = "{";
            jsonData += "\"username\": \"" + user.get_username() + "\",";
            jsonData += "\"avatar\": \"" + user.get_avatar() + "\",";
            jsonData += "\"email\": \"" + user.get_eamil() + "\",";
            jsonData += "\"registerDate\": \"" + user.get_register_time() + "\",";
            jsonData += "\"bio\": \"" + user.get_description() + "\",";
            jsonData += "\"articleCount\": \"" + to_string(article_count) + "\",";
            jsonData += "\"viewCount\": \"" + to_string(view_count) + "\",";
            jsonData += "\"likeCount\": \"" + to_string(like_count) + "\",";
            jsonData += "\"commentCount\": \"" + to_string(comment_count) + "\"";
            jsonData += "}";

            return BLOG_DATA;
        }else{
            jsonData = "{\"message\": \"未登录或会话已过期\"}";
            return AUTHENTICATION;
        }
    }
    // 获取文章信息
    else if (strncmp(m_url, "/api/article/", 13) == 0 && isdigit(m_url[13])) {
        char* articleIdParam = strstr(m_url, "/api/article/") + 13;
        if(articleIdParam == nullptr){
            return BAD_REQUEST;
        }
        int articleId = atoi(articleIdParam);
        if(articleId <= 0){
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        Blog blog = tool.select_blog_by_id(articleId);
        string escapedTitle = escapeJsonString(blog.get_blog_title());
        string escapedContent = escapeJsonString(blog.get_blog_content());
        User user = tool.get_userdata_by_userid(blog.get_user_id());
        string category = tool.get_cotegoriename_by_cotegorieid(blog.get_category_id());
        vector<string> tags = tool.get_tags_by_blogid(articleId);
        int likes = tool.get_blog_likes_count(articleId);
        int comments = tool.get_blog_comments_count(articleId);
        Blog prevPost = tool.get_prev_blog_by_id(articleId);
        Blog nextPost = tool.get_next_blog_by_id(articleId);

        // 先创建文章对象
        json article = {
            {"id", blog.get_blog_id()},
            {"title", escapedTitle},
            {"content", blog.get_blog_content()},
            {"date", blog.get_blog_postTime()},
            {"thumbnail", blog.get_thumbnail()},
            {"category", category},
            {"tags", json::array()}, // 创建一个空数组，然后添加标签
            {"author", user.get_username()},
            {"authorAvatar", user.get_avatar()},
            {"authorBio", user.get_description()},
            {"views", blog.get_views()},
            {"likes", likes},
            {"comments", comments},
            {"prevPost", {
                {"id", prevPost.get_blog_id()},
                {"title", prevPost.get_blog_title()}
            }},
            {"nextPost", {
                {"id", nextPost.get_blog_id()},
                {"title", nextPost.get_blog_title()}
            }}
        };

        // 将标签添加到数组
        for (const auto& tag : tags) {
            article["tags"].push_back(tag);
        }

        // 然后创建最终的响应对象
        json response = {
            {"article", article}
        };

        jsonData = response.dump();
        tool.increase_blog_view_count(articleId);
        return BLOG_DATA;
    }
    // 获取评论列表
    else if (strstr(m_url, "/api/comments?articleId=")) {
        char* articleIdParam = strstr(m_url, "articleId=");
        if(articleIdParam == nullptr){
            return BAD_REQUEST;
        }
        int articleId = atoi(articleIdParam + 10);
        if(articleId <= 0){
            return BAD_REQUEST;
        }

        sql_blog_tool tool;

        // 获取评论列表
        vector<Comments> comments = tool.get_comments_by_blogid(articleId);
        
        // 创建评论数组
        json commentsArray = json::array();
        
        // 遍历评论列表构建json
        for(auto& comment : comments) {
            // 获取评论用户信息
            int userid = tool.get_userid(comment.get_username());
            User user = tool.get_userdata_by_userid(userid);

            
            json commentObj = {
                {"id", comment.get_comment_id()},
                {"username", user.get_username()},
                {"avatar", user.get_avatar()}, 
                {"content", comment.get_content()},
                {"date", comment.get_comment_time()},
                {"likes", 0} // 暂时写死为0
            };
            commentsArray.push_back(commentObj);
        }
        
        // 构建响应json
        json response = {
            {"comments", commentsArray}
        };

        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 获取相关文章
    else if (strstr(m_url, "/api/article/related")) {
        string category = "";
        int excludeId = -1;
        int size = 5;

        // 解析请求参数
        char* categoryParam = strstr(m_url, "category=");
        char* excludeIdParam = strstr(m_url, "excludeId=");
        char* sizeParam = strstr(m_url, "size=");

        if(categoryParam) {
            category = string(categoryParam + 9);
            size_t ampPos = category.find("&");
            if (ampPos != string::npos) {
                category = category.substr(0, ampPos);
            }
            category = url_decode(category);
        }

        if(excludeIdParam) {
            excludeId = std::atoi(excludeIdParam + 10);
            if(excludeId <= 0) excludeId = -1;
        }

        if(sizeParam) {
            size = std::atoi(sizeParam + 5);
            if(size <= 0 || size > 100) size = 6;
        }
        
        sql_blog_tool tool;
        int categoryId = tool.get_category_id_by_name(category);
        if(categoryId == -1){
            return BAD_REQUEST;
        }
        vector<Blog> relatedBlogs = tool.get_related_blogs(categoryId, excludeId, size);
        
        // 构建响应json
        json response = {
            {"articles", json::array()}
        };
        
        // 遍历相关文章构建json
        for(auto& blog : relatedBlogs) {
            User user = tool.get_userdata_by_userid(blog.get_user_id());
            int likes = tool.get_blog_likes_count(blog.get_blog_id());
            json blogObj = {
                {"id", blog.get_blog_id()},
                {"title", blog.get_blog_title()},
                {"thumbnail", blog.get_thumbnail()},
                {"views", blog.get_views()},
                {"likes", likes}
            };
            response["articles"].push_back(blogObj);
        }
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 点赞(取消点赞)博客
    else if (strncmp(m_url, "/api/article/like/", 18) == 0 && isdigit(m_url[18])) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "/api/article/like/") + 18;
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        int blogId = atoi(blogIdStart);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        json request = json::parse(m_string);
        bool isLiked = request["liked"];

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        
        if(isLiked){
            if(tool.is_user_liked_blog(userid, blogId)){
                json response = {
                    {"success", false},
                    {"message", "您已点赞过该文章"},
                    {"likeCount", tool.get_blog_likes_count(blogId)}
                };
                jsonData = response.dump();
            }else{
                tool.insert_new_blog_like(userid, blogId);
                json response = {
                    {"success", true},
                    {"message", "点赞成功"},
                    {"likeCount", tool.get_blog_likes_count(blogId)}
                };
                jsonData = response.dump();
            }
        }else{
            if(tool.is_user_liked_blog(userid, blogId)){
                tool.remove_blog_like(userid, blogId);
                json response = {
                    {"success", true},
                    {"message", "取消点赞成功"},
                    {"likeCount", tool.get_blog_likes_count(blogId)}
                };
                jsonData = response.dump();
            }else{
                json response = {
                    {"success", false},
                    {"message", "您未点赞过该文章"},
                    {"likeCount", tool.get_blog_likes_count(blogId)}
                };
                jsonData = response.dump();
            }
        }
        return BLOG_DATA;
    }
    // 获取用户与文章的互动状态
    else if (strncmp(m_url, "/api/article/interactions/", 26) == 0 && isdigit(m_url[26])) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "/api/article/interactions/");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }

        int blogId = atoi(blogIdStart + 26);
        if (blogId <= 0) {
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        bool isLiked = tool.is_user_liked_blog(userid, blogId);

        json response = {
            {"liked", isLiked},
            {"bookmarked", false}
        };

        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 发表评论
    else if (m_method == POST && strstr(m_url, "/api/comments") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        json request = json::parse(m_string);
        string content = request["content"];
        string articleId = request["articleId"];

        if(articleId.empty()){
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int commentId = tool.add_comment_to_article(username, stoi(articleId), content);
        if(commentId != -1){
            // 获取系统时间
            time_t now = time(nullptr);
            tm* local_time = localtime(&now);
            char buffer[100];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);
            string postTime = string(buffer);
            json response = {
                {"success", true},
                {"message", "评论发表成功"},
                {"commentId", commentId},
                {"date", postTime}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }

        jsonData = "{\"success\": false, \"message\": \"评论失败\"}";
        return BLOG_DATA;
    }
    // 文章发布
    else if (m_method == POST && strstr(m_url, "/api/articles") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        sql_blog_tool tool;

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 解析multipart/form-data格式的请求体
        if(m_boundary.empty()){
            return BAD_REQUEST;
        }

        std::map<std::string, std::string> form_data;
        std::map<std::string, file_data_t> files;
        if(!parse_multipart_form_data(m_boundary, form_data, files)){
            return BAD_REQUEST;
        }

        if(form_data.find("title") == form_data.end()){
            json response = {
                {"success", false},
                {"message", "标题不能为空"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }
        if(form_data.find("content") == form_data.end()){
            json response = {
                {"success", false},
                {"message", "内容不能为空"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }

        string title = form_data["title"];
        string content = form_data["content"];
        string category = "";
        string tags = "";
        string thumbnail_path = "";
        int categoryId = 1;
        // 获取category
        if(form_data.find("category") != form_data.end()){
            category = form_data["category"];
            categoryId = tool.get_category_id_by_name(category);
            if(categoryId == -1){
                json response = {
                    {"success", false},
                    {"message", "分类不存在"}
                };
                jsonData = response.dump();
                return BLOG_DATA;
            }
        }
        // 获取thumbnail
        if(files.find("thumbnail") != files.end()){
            file_data_t thumbnail = files["thumbnail"];
            char filename[100] = {0};
            // sprintf(filename, "thumbnail/%s.%s", article_id, get_file_extension(thumbnail.filename).c_str());
            sprintf(filename, "/root/projects/C-WebServer/root/thumbnail/%s", generate_unique_filename(thumbnail.filename).c_str());
            
            // 保存文件
            FILE *fp = fopen(filename, "wb");
            if (fp) {
                fwrite(thumbnail.data, 1, thumbnail.size, fp);
                fclose(fp);
                // 只提取路径中的/thumbnail/部分
                size_t pos = string(filename).find("/thumbnail/");
                if (pos != string::npos) {
                    thumbnail_path = string(filename).substr(pos);
                } else {
                    thumbnail_path = "/thumbnail/" + string(generate_unique_filename(thumbnail.filename));
                }
            }
        }
        int userid = tool.get_userid(username);
        int blogId = tool.add_blog(title, content, userid, categoryId, thumbnail_path);
        if(blogId == -1){
            json response = {
                {"success", false},
                {"message", "文章发布失败"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }

        // 获取tags
        if(form_data.find("tags") != form_data.end()){
            tags = form_data["tags"];
        }
        if(!tags.empty()){
            // 解析tags
            json tags_json = json::parse(tags);
            for(const auto& tag : tags_json.items()){
                string tag_name = tag.value();
                int tagId = tool.get_tag_id_by_name(tag_name);
                if(tagId != -1){
                    tool.add_blog_tag(blogId, tagId);
                }else{
                    int newTagId = tool.create_tag(tag_name);
                    tool.add_blog_tag(blogId, newTagId);
                }
            }
        }

        json response = {
            {"success", true},
            {"message", "文章发布成功"},
            {"articleId", blogId}
        };
        jsonData = response.dump();

        return BLOG_DATA;
        
    }
    // 处理图片上传
    else if (m_method == POST && strstr(m_url, "/api/upload/image") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 解析multipart/form-data
        if(m_boundary.empty()){
            return BAD_REQUEST;
        }

        std::map<std::string, std::string> form_data;
        std::map<std::string, file_data_t> files;
        if(!parse_multipart_form_data(m_boundary, form_data, files)){
            return BAD_REQUEST;
        }

        if(files.find("image") == files.end()){
            json response = {
                {"success", false},
                {"message", "未找到图片文件"}
            };
            jsonData = response.dump();
            return BLOG_DATA;
        }

        file_data_t image = files["image"];
        char filename[100] = {0};
        sprintf(filename, "/root/projects/C-WebServer/root/blog_images/%s", 
                generate_unique_filename(image.filename).c_str());
        
        // 保存文件
        FILE *fp = fopen(filename, "wb");
        if (fp) {
            fwrite(image.data, 1, image.size, fp);
            fclose(fp);
            
            // 生成访问URL
            string image_url = "/blog_images/" + string(generate_unique_filename(image.filename));
            
            json response = {
                {"success", true},
                {"url", image_url}
            };
            jsonData = response.dump();
        } else {
            json response = {
                {"success", false},
                {"message", "图片保存失败"}
            };
            jsonData = response.dump();
        }

        return BLOG_DATA;
    }
    // 获取标签列表
    else if (strstr(m_url, "/api/tags") != nullptr) {
        sql_blog_tool tool;
        vector<string> tagNames = tool.get_all_tags(); // 获取所有标签名称
        
        json response = {
            {"code", 200},
            {"message", "获取标签列表成功"},
            {"tags", json::array()}
        };

        // 获取每个标签的博客数量并构建响应
        for (const auto& tagName : tagNames) {
            Tags tag = tool.get_tag_by_tagname(tagName);
            int tagId = tag.get_id();
            int blogCount = tool.get_total_blog_count_by_tag(tagId);
            
            response["tags"].push_back({
                {"id", tagId},
                {"name", tagName},
                {"count", blogCount}
            });
        }

        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-留言板-获取留言列表
    else if (m_method == GET && strstr(m_url, "/api/messages_board") != nullptr) {
        // 解析页码和每页数量参数
        int page = 1;
        int size = 10;
        
        // 获取page参数
        const char* pageParam = strstr(m_url, "page=");
        if (pageParam != nullptr) {
            page = atoi(pageParam + 5);
        }
        
        // 获取size参数
        const char* sizeParam = strstr(m_url, "size=");
        if (sizeParam != nullptr) {
            size = atoi(sizeParam + 5);
        }
        
        // 确保参数有效
        if (page < 1) page = 1;
        if (size < 1) size = 10;
        
        // 查询数据库获取留言列表
        sql_blog_tool tool;
        vector<MessageBoard> messages = tool.get_message_board_by_page(page, size);
        
        // 获取总留言数
        int total_messages = tool.get_total_message_board_count();
        int total_pages = (total_messages + size - 1) / size; // 计算总页数
        
        // 构造JSON响应
        json response = {
            {"messages", json::array()},
            {"hasMore", page < total_pages},
            {"totalCount", total_messages},
            {"currentPage", page},
            {"totalPages", total_pages}
        };
        
        // 创建消息ID到消息对象的映射，用于构建父子关系
        map<int, json> messageMap;
        
        for (auto& message : messages) {
            // 获取作者信息
            User user = tool.get_userdata_by_userid(message.get_user_id());
            
            // 获取点赞数
            int likes = tool.get_message_likes_count(message.get_message_id());
            
            // 检查当前用户是否点赞
            bool isLiked = false;
            if (user.get_userid() > 0) {
                isLiked = tool.is_user_liked_message(user.get_userid(), message.get_message_id());
            }
            
            // 获取父消息ID
            int parentId = message.get_parent_id();
            if(parentId == 0){
                parentId = -1;
            }
            
            json messageObj = {
                {"id", message.get_message_id()},
                {"parentId", parentId},
                {"author", user.get_username()},
                {"authorAvatar", user.get_avatar()},
                {"content", message.get_content()},
                {"date", message.get_date()},
                {"likes", likes},
                {"isLiked", isLiked}
            };
            
            // 将消息添加到响应中
            response["messages"].push_back(messageObj);
            
            // 同时保存到映射中，以便后续处理回复关系
            messageMap[message.get_message_id()] = messageObj;
        }
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-留言板-发表留言
    else if (m_method == POST && strcmp(m_url, "/api/messages_board") == 0) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        // 验证用户登录状态
        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 解析POST请求体
        json post_data;
        try {
            post_data = json::parse(m_string);
        } catch (const json::parse_error& e) {
            jsonData = "{\"success\": false, \"message\": \"请求格式错误\"}";
            return BAD_REQUEST;
        }

        // 检查必要字段
        if (!post_data.contains("content") || post_data["content"].empty()) {
            jsonData = "{\"success\": false, \"message\": \"留言内容不能为空\"}";
            return BAD_REQUEST;
        }

        string content = post_data["content"];
        int parentIdInt = -1;
        
        // 检查parentId是否存在并正确处理类型
        if (post_data.contains("parentId")) {
            if (post_data["parentId"].is_number()) {
                parentIdInt = post_data["parentId"];
            } else if (post_data["parentId"].is_string()) {
                try {
                    parentIdInt = stoi(post_data["parentId"].get<string>());
                } catch (const exception& e) {
                    // 转换失败时使用默认值-1
                    parentIdInt = -1;
                }
            }
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        int message_id = tool.add_message_board(userid, content, parentIdInt);

        // 获取用户信息
        User user = tool.get_userdata_by_userid(userid);
        MessageBoard message_board = tool.get_message_board_by_id(message_id);

        // 构建响应
        json messageObj = {
            {"id", message_id},
            {"parentId", parentIdInt},
            {"author", user.get_username()},
            {"authorAvatar", user.get_avatar()},
            {"content", message_board.get_content()},
            {"date", message_board.get_date()},
            {"likes", 0},
            {"isLiked", false}
        };

        json response = {
            {"message", messageObj}
        };
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-留言板-删除留言
    else if (m_method == DELETE && strstr(m_url, "/api/messages_board/") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        
        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 从URL中提取留言ID
        const char* idStart = strstr(m_url, "/api/messages_board/") + strlen("/api/messages_board/");
        int messageId = atoi(idStart);
        
        if (messageId <= 0) {
            jsonData = "{\"success\": false, \"message\": \"无效的留言ID\"}";
            return BAD_REQUEST;
        }

        // 删除留言
        sql_blog_tool tool;
        bool success = tool.delete_message_board(messageId);

        // 构建响应
        json response = {
            {"success", success},
            {"message", success ? "留言删除成功" : "删除失败"}
        };
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-留言板-点赞留言
    else if (m_method == POST && strstr(m_url, "/api/messages_board/like/") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 从URL中提取留言ID
        const char* idStart = strstr(m_url, "/api/messages_board/like/") + strlen("/api/messages_board/like/");
        int messageId = atoi(idStart);
        
        if (messageId <= 0) {
            jsonData = "{\"success\": false, \"message\": \"无效的留言ID\"}";
            return BLOG_DATA;
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        json post_data;
        bool success = false;
        bool isLiked = false;
        
        try {
            post_data = json::parse(m_string);
            isLiked = post_data["liked"];
            
            if(isLiked){
                success = tool.insert_new_message_like(userid, messageId);
            }else{
                success = tool.remove_message_like(userid, messageId);
            }
        } catch (const json::parse_error& e) {
            jsonData = "{\"success\": false, \"message\": \"请求格式错误\"}";
            return BLOG_DATA;
        }

        // 获取点赞数
        int likes = tool.get_message_likes_count(messageId);

        // 构建响应
        json response = {
            {"isLiked", isLiked},
            {"likes", likes}
        };
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-留言板-对留言进行回复
    else if (m_method == POST && strncmp(m_url, "/api/messages_board/replies/", 28) == 0 && isdigit(m_url[28])) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        
        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }
        
        // 从URL中提取父留言ID
        const char* idStart = strstr(m_url, "/api/messages_board/replies/") + strlen("/api/messages_board/replies/");
        int parentIdFromUrl = atoi(idStart);
        
        // 解析POST请求体
        json post_data;
        try {
            post_data = json::parse(m_string);
        } catch (const json::parse_error& e) {
            jsonData = "{\"success\": false, \"message\": \"请求格式错误\"}";
            return BAD_REQUEST;
        }

        // 检查必要字段
        if (!post_data.contains("content") || post_data["content"].empty()) {
            jsonData = "{\"success\": false, \"message\": \"回复内容不能为空\"}";
            return BAD_REQUEST;
        }

        string content = post_data["content"];
        int parentIdInt = parentIdFromUrl; // 优先使用URL中的ID
        
        // 如果请求体中也有parentId，则使用请求体中的值
        if (post_data.contains("parentId")) {
            if (post_data["parentId"].is_number()) {
                parentIdInt = post_data["parentId"];
            } else if (post_data["parentId"].is_string()) {
                try {
                    parentIdInt = stoi(post_data["parentId"].get<string>());
                } catch (const exception& e) {
                    // 保持使用URL中的ID
                }
            }
        }
        
        // 验证父留言ID的有效性
        if (parentIdInt <= 0) {
            jsonData = "{\"success\": false, \"message\": \"无效的留言ID\"}";
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        
        // 添加回复
        try {
            int replyId = tool.add_message_board(userid, content, parentIdInt);
            
            if (replyId <= 0) {
                jsonData = "{\"success\": false, \"message\": \"添加回复失败\"}";
                return INTERNAL_ERROR;
            }
            
            // 获取回复信息
            MessageBoard reply = tool.get_message_board_by_id(replyId);
            User user = tool.get_userdata_by_userid(userid);

            // 构建响应
            json messageObj = {
                {"id", replyId},
                {"parentId", parentIdInt},
                {"author", user.get_username()},
                {"authorAvatar", user.get_avatar()},
                {"content", reply.get_content()},
                {"date", reply.get_date()},
                {"likes", 0},
                {"isLiked", false}
            };

            json response = {
                {"reply", messageObj}
            };
            
            jsonData = response.dump();
            return BLOG_DATA;
        } catch (const exception& e) {
            jsonData = "{\"success\": false, \"message\": \"服务器内部错误\"}";
            return INTERNAL_ERROR;
        }
    }
    // 用户功能-留言板-对回复进行点赞
    else if (m_method == POST && strstr(m_url, "/api/messages_board/replies/like/") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 从URL中提取回复ID
        const char* idStart = strstr(m_url, "/api/messages_board/replies/like/") + strlen("/api/messages_board/replies/like/");
        int replyId = atoi(idStart);
        
        if (replyId <= 0) {
            jsonData = "{\"success\": false, \"message\": \"无效的回复ID\"}";
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);
        json post_data;
        bool success = false;
        bool isLiked = false;
        
        try {
            post_data = json::parse(m_string);
            isLiked = post_data["liked"];
            
            if(isLiked){
                success = tool.insert_new_message_like(userid, replyId);
            }else{
                success = tool.remove_message_like(userid, replyId);
            }
        } catch (const json::parse_error& e) {
            jsonData = "{\"success\": false, \"message\": \"请求格式错误\"}";
            return BLOG_DATA;
        }

        // 获取点赞数
        int likes = tool.get_message_likes_count(replyId);

        // 构建响应
        json response = {
            {"isLiked", isLiked},
            {"likes", likes}
        };
        
        jsonData = response.dump();
        return BLOG_DATA;
    }
    // 用户功能-修改密码
    else if (m_method == PATCH && strcmp(m_url, "/api/user/update_password") == 0) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            jsonData = "{\"success\": false, \"message\": \"请先登录后再操作\"}";
            return AUTHENTICATION;
        }

        // 解析POST请求体
        json post_data;
        try {
            post_data = json::parse(m_string);
        } catch (const json::parse_error& e) {
            jsonData = "{\"success\": false, \"message\": \"请求格式错误\"}";
            return BAD_REQUEST;
        }

        string oldPassword = post_data["oldPassword"];
        string newPassword = post_data["newPassword"];
        string hash_password = BCrypt::generateHash(newPassword);

        sql_blog_tool tool;
        if(users[username] == oldPassword || BCrypt::validatePassword(oldPassword, users[username])){
            tool.modify_password_by_username(username, hash_password);
            users.insert(pair<string, string>(username, hash_password));
            jsonData = "{\"success\": true, \"message\": \"密码修改成功\"}";
        }else{
            jsonData = "{\"success\": false, \"message\": \"原密码不正确\"}";
        }

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

        tool.increase_blog_view_count(blog.get_blog_id());

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
    // 发表评论-同时同步该消息到信息表
    else if (strstr(m_url, "/add_comment") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id))
             return BAD_REQUEST;

        string post_body = url_decode(m_string);
        auto post_data = parse_post_data(post_body);
        
        string blog_id = post_data["blogId"];
        string content = post_data["content"];

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

        // 将评论信息插入消息表
        Messages message;
        int userid = tool.get_userid(username);
        message.set_sender_id(userid);
        message.set_blog_id(stoi(blog_id));
        message.set_type("评论通知");
        message.set_content(content);
        message.set_post_time(postTime);
        message.set_is_read(0);
        int recipient_id = tool.get_userid_by_blogid(stoi(blog_id));
        message.set_recipient_id(recipient_id);

        tool.insert_new_message(message);

        return REDIRECT_HOME;
    }
    // 博客详情-点赞-检查用户是否已经点赞
    else if (strstr(m_url, "/check_blog_like") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(!cookie.validateSession(username, session_id)){
            return BAD_REQUEST;
        }

        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blog_id = atoi(blogIdStart + 7);
        if (blog_id <= 0) {
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int userid = tool.get_userid(username);

        // 查询当前用户对该博客的点赞状态
        bool isLiked = tool.is_user_liked_blog(userid, blog_id);
        int likeCount = tool.get_blog_likes_count(blog_id);

        jsonData = "{\"success\": true, \"isLiked\": " + to_string(isLiked) + ", \"likeCount\": " + to_string(likeCount) + "}";
        return BLOG_DATA;
    }
    // 博客详情-点赞
    else if (strstr(m_url, "/like_blog") != nullptr) {
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        // 验证用户的会话
        if (!cookie.validateSession(username, session_id))
            return BAD_REQUEST;

        // 解析POST请求体
        string post_body = url_decode(m_string);
        auto post_data = parse_post_data(post_body);

        string blog_id = post_data["blogId"];
        string action = post_data["action"];  // like 或 unlike

        sql_blog_tool tool;
        int userid = tool.get_userid(username);

        bool success = false;
        if (action == "like") {
            // 判断用户是否已点赞过
            bool already_liked = tool.is_user_liked_blog(userid, stoi(blog_id));

            if (!already_liked) {
                // 用户未点赞，进行点赞操作
                success = tool.insert_new_blog_like(userid, stoi(blog_id));
            } else {
                // 用户已经点赞过，返回错误
                jsonData = "{\"status\": \"error\", \"message\": \"你已经点赞过此博客\"}";
                return BLOG_DATA;
            }

        } else if (action == "unlike") {
            // 判断用户是否已点赞
            bool already_liked = tool.is_user_liked_blog(userid, stoi(blog_id));

            if (already_liked) {
                // 用户已点赞，进行取消点赞操作
                success = tool.remove_blog_like(userid, stoi(blog_id));
            } else {
                // 用户没有点赞过，返回错误
                jsonData = "{\"status\": \"error\", \"message\": \"你尚未点赞此博客\"}";
                return BLOG_DATA;
            }
        }

        // 返回响应
        if (success) {
            // 获取更新后的点赞数
            int new_like_count = tool.get_blog_likes_count(stoi(blog_id));

            jsonData = "{\"status\": \"success\", \"likeCount\": " + to_string(new_like_count) + "}";
            return BLOG_DATA;
        } else {
            // 处理失败，返回错误信息
            jsonData = "{\"status\": \"error\", \"message\": \"操作失败，请稍后重试\"}";
            return BAD_REQUEST;
        }
    }
    // 博客详情-获取当前博客总点赞数
    else if (strstr(m_url, "/get_blog_likes_count") != nullptr) {
        // 解析 blogId
        const char* blogIdStart = strstr(m_url, "blogId=");
        if (blogIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int blog_id = atoi(blogIdStart + 7);
        if (blog_id <= 0) {
            return BAD_REQUEST;
        }

        sql_blog_tool tool;
        int blog_like_count = tool.get_blog_likes_count(blog_id);

        jsonData = "{\"success\": true, \"likeCount\": " + std::to_string(blog_like_count) + "}";
        return BLOG_DATA;
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
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/blog_home.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if(strstr(m_url, "blog_user_home.html")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            const char* blog_path = "/blog_user_home.html";
            size_t path_len = strlen(blog_path);
            
            // 确保 m_real_file 有足够空间
            if(len + path_len < 200) {  // 假设 m_real_file 总大小为200
                strcpy(m_real_file + len, blog_path);
            } else {
                // 处理路径过长的情况
                return BAD_REQUEST;
            }
        } else {
            cookie.removeSession(username);
            return REDIRECT_HOME;
        }
    }
    else if (strstr(m_url, "/logout_user")){     // 注销并重定向到登录界面
        string username = cookie.getCookie("username");
        cookie.removeSession(username);
        return REDIRECT;
    }
    // else if (strstr(m_url, "/comments"))
    // {
    //     // 创建 JSON 格式的测试数据
    //     jsonData = R"([
    //         {
    //             "text": "这是第一条留言",
	// 	        "timestamp": "2024-11-13 3:03:14"
    //         },
    //         {
    //             "text": "这是第二条留言",
	// 	        "timestamp": "2024-11-13 16:03:14"
    //         }
    //     ])";
        
    //     // 将 JSON 数据加入响应
    //     return BLOG_DETAIL;
    // }
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
    // 个人中心-个人资料-头像上传
    else if (strstr(m_url, "/upload_avatar")) {
        // 验证用户的登录状态
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if (!cookie.validateSession(username, session_id)) {
            return BAD_REQUEST;
        }

        // 解析上传的 multipart/form-data ff
        string file_path = handle_file_upload(m_boundary, m_post_content, "/root/projects/C-WebServer/root/img");

        if (file_path.empty()) {
            jsonData = "{\"success\": false, \"message\": \"上传失败，文件无效\"}";
            return BAD_REQUEST;
        }

        // 将文件路径保存到数据库
        size_t pos = file_path.find("/img");
        file_path = file_path.substr(pos);
        
        sql_blog_tool tool;
        if (!tool.update_avatar_path(username, file_path)) {
            jsonData = "{\"success\": false, \"message\": \"数据库更新失败\"}";
            return INTERNAL_ERROR;
        }

        // 返回成功响应
        jsonData = "{\"success\": true, \"avatarUrl\": \"" + file_path + "\"}";
        return BLOG_DATA;
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
    // 个人中心-消息中心
    else if (strstr(m_url, "/messages")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");

        if(cookie.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 根据用户名获取用户id
            int userid = tool.get_userid(username);

            // 根据用户id获取该用户的全部消息
            vector<Messages> messages;
            messages = tool.get_messages_by_userid(userid);

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
            jsonData += "\"messages\": [";
            for (int i = 0; i < messages.size(); i++) {
                Messages message = messages[i];
                string escapedContent = escapeJsonString(message.get_content());
                jsonData += "{";
                jsonData += "\"id\": " + to_string(message.get_message_id()) + ",";
                jsonData += "\"type\": \"" + message.get_type() + "\",";
                jsonData += "\"content\": \"" + message.get_content() + "\",";
                jsonData += "\"postTime\": \"" + message.get_post_time() + "\",";
                jsonData += "\"isRead\": \"" + to_string(message.get_is_read()) + "\",";
                jsonData += "\"relatedLink\": \"" + to_string(message.get_blog_id()) + "\"";
                jsonData += "}";
                if (i < messages.size() - 1) jsonData += ",";
            }
            jsonData += "]";
            jsonData += "}";

            return BLOG_DATA;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 个人中心-消息中心-标记单个消息为已读
    else if (strstr(m_url, "/mark_message_read")){
        // 解析 messageId
        const char* messageIdStart = strstr(m_url, "messageId=");
        if (messageIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int messageId = atoi(messageIdStart + 10);
        if (messageId <= 0) {
            return BAD_REQUEST;
        }

        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        sql_blog_tool tool;
        int userid = tool.get_userid(username);

        if(cookie.validateSession(username, session_id) && tool.check_message_belongs_to_user(userid, messageId)){
            // 标记消息为已读
            bool success = tool.mark_message_as_read(messageId);

            if(success){
                jsonData = "{\"status\": \"success\"}";
                return BLOG_DATA;
            }else{
                jsonData = "{\"status\": \"error\", \"message\": \"标记消息失败\"}";
                return BAD_REQUEST;
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 个人中心-消息中心-标记所有消息为已读
    else if (strstr(m_url, "/mark_all_messages_read")){
        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        sql_blog_tool tool;
        int userid = tool.get_userid(username);

        if(cookie.validateSession(username, session_id)){
            // 标记所有消息为已读
            bool success = tool.mark_all_message_as_read(userid);

            if(success){
                jsonData = "{\"status\": \"success\"}";
                return BLOG_DATA;
            }else{
                jsonData = "{\"status\": \"error\", \"message\": \"标记所有消息失败\"}";
                return BAD_REQUEST;
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 个人中心-消息中心-删除指定消息
    else if (strstr(m_url, "/delete_message")){
        // 解析 messageId
        const char* messageIdStart = strstr(m_url, "messageId=");
        if (messageIdStart == nullptr) {
            return BAD_REQUEST;
        }
        
        int messageId = atoi(messageIdStart + 10);
        if (messageId <= 0) {
            return BAD_REQUEST;
        }

        string username = cookie.getCookie("username");
        string session_id = cookie.getCookie("session_id");
        sql_blog_tool tool;
        int userid = tool.get_userid(username);

        if(cookie.validateSession(username, session_id) && tool.check_message_belongs_to_user(userid, messageId)){
            // 标记消息为已读
            bool success = tool.delete_message(messageId);

            if(success){
                jsonData = "{\"status\": \"success\"}";
                return BLOG_DATA;
            }else{
                jsonData = "{\"status\": \"error\", \"message\": \"删除消息失败\"}";
                return BAD_REQUEST;
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-登录逻辑
    else if(strstr(m_url, "/admins/admins_login")){
        // 避免重复登录
        string user = cookie_admin.getCookie("admin_username");
        string session_id = cookie_admin.getCookie("admin_session_id");
        // 若已登录则直接返回主页
        if(cookie_admin.validateSession(user, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/admins/admin_homepage.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else{
            // 对请求体的内容进行解码
            string body = url_decode(m_string);

            // 获取请求体的内容
            auto post_data = parse_post_data(body);

            // 获取用户名和密码
            string username = post_data["username"];
            string password = post_data["password"];

            // 从数据库中获取对应username的密码
            sql_blog_tool tool;
            string stored_hash = tool.get_admin_password_by_username(username);

            // 验证密码 插眼 这里后面也要删
            if(BCrypt::validatePassword(password, stored_hash) || password == stored_hash){
                // 为当前的用户创建新会话
                char *m_url_real = (char *)malloc(sizeof(char) * 200);
                try{
                    cookie_admin.createSession(username);
                    current_username = username;
                    islogin = true;
                    strcpy(m_url, "/admins/admin_homepage.html");
                    strcpy(m_url_real, "/admins/admin_homepage.html");
                } catch(const runtime_error& e){    // 若当前用户已登录，再登录会返回错误页面(用户已登录)
                    strcpy(m_url, "/blog_loginErrorExit.html");
                    strcpy(m_url_real, "/blog_loginErrorExit.html");
                }

                strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
                free(m_url_real);
            }else{

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
            return ADMIN_LOGIN_REQUEST;
        }
    }
    // 管理员-登录页面
    else if(strstr(m_url, "/admins/login")){
        // 检验是否登录 已登录：跳转主页 未登录：跳转登录页面
        // 可以通过 检测请求中有没有cookie 判定是否跳转到登录界面
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");
        // 若为空说明没登录（有可能已登录，cookie有保留）
        if(session_id.empty() || username.empty()){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/admins/admin_login.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else{  
            // 若不为空，则检验会话是否有效
            if(cookie_admin.validateSession(username, session_id)){
                char *m_url_real = (char *)malloc(sizeof(char) * 200);
                strcpy(m_url_real, "/admins/admin_homepage.html");
                strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

                free(m_url_real);
            }else{
                // 若客户端发来的cookie无效，则将其的cookie设置失效
                cookie_admin.forceClearClientCookies();
                return REDIRECT_ADMIN;
            }
        }
        
    }
    // 管理员-主页面
    else if(strstr(m_url, "/admins/admin_homepage.html")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            char *m_url_real = (char *)malloc(sizeof(char) * 200);
            strcpy(m_url_real, "/admins/admin_homepage.html");
            strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

            free(m_url_real);
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员注销
    else if (strstr(m_url, "/logout_admin")){     // 注销并重定向到登录界面
        string username = cookie_admin.getCookie("username");
        cookie_admin.removeSession(username);
        return REDIRECT_ADMIN;
    }
    // 管理员-仪表盘
    else if(strstr(m_url, "/admins/stats")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            sql_blog_tool tool;
            int blog_count = tool.get_total_blog_count();
            int user_count = tool.get_user_count();
            int comment = tool.get_comment_count();
            int like = tool.get_like_count();

            jsonData = "{";
            jsonData += "\"totalPosts\":" + to_string(blog_count) + ",";
            jsonData += "\"totalUsers\":" + to_string(user_count) + ",";
            jsonData += "\"totalComments\":" + to_string(comment) + ",";
            jsonData += "\"totalLikes\":" + to_string(like);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-文章管理-获取文章列表
    else if(m_method == GET && strstr(m_url, "/admins/posts?")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            int page = 1, size = 10;
            string sortField = "postTime"; // 默认排序字段(关键词) 按时间or按名称
            string sortOrder = "desc"; // 默认排序方式 升序or降序
            int categoryId = -1; // 默认不筛选分类
            string searchKeyword = ""; // 默认无搜索

            // 解析url参数
            char* pageParam = strstr(m_url, "page=");
            char* sizeParam = strstr(m_url, "pageSize=");
            char* sortParam = strstr(m_url, "sort=");
            char* categoryIdParam = strstr(m_url, "categoryId=");
            char* searchParam = strstr(m_url, "search=");

            if (pageParam) {
                page = std::atoi(pageParam + 5);
                if (page <= 0) page = 1;
            }
            if (sizeParam) {
                size = std::atoi(sizeParam + 9);
                if (size <= 0 || size > 100) size = 20;
            }
            if (sortParam) {
                string sortStr = string(sortParam + 5);
                size_t pos = sortStr.rfind("_");
                if (pos != string::npos) {
                    sortField = sortStr.substr(0, pos);
                    sortOrder = sortStr.substr(pos + 1);

                    // 防止将后续的 &categoryId=1 作为排序顺序的一部分
                    size_t ampPos = sortOrder.find("&");
                    if (ampPos != string::npos) {
                        sortOrder = sortOrder.substr(0, ampPos);  // 只取排序方向部分
                    }
                }
            }
            if (categoryIdParam) {
                categoryId = std::atoi(categoryIdParam + 11); // 解析 categoryId 参数
                if (categoryId <= 0) categoryId = -1; // 如果 categoryId 非法，返回-1，不做筛选
            }
            if (searchParam) {
            searchKeyword = string(searchParam + 7);
            size_t ampPos = searchKeyword.find("&");
            if (ampPos != string::npos) {
                searchKeyword = searchKeyword.substr(0, ampPos);
            }
            searchKeyword = url_decode(searchKeyword);  // 进行 URL 解码
            }
            
            // 查询数据库
            sql_blog_tool tool;
            vector<Blog> blogs;
            int totalCount = 0;

            if (!searchKeyword.empty()) {
                if (categoryId != -1) {
                    // 分类 + 关键词搜索
                    cout << "111" << endl;
                    blogs = tool.get_blogs_by_category_and_search(categoryId, searchKeyword, page, size, sortField, sortOrder);
                    totalCount = tool.get_total_blog_count_by_category_and_search(categoryId, searchKeyword);
                } else {
                    // 仅搜索
                    cout << "222" << endl;
                    blogs = tool.get_blogs_by_search(page, size, sortField, sortOrder, searchKeyword);
                    totalCount = tool.get_total_blog_count_by_search(searchKeyword);
                }
            } else {
                if (categoryId != -1) {
                    // 仅分类
                    cout << "333" << endl;
                    blogs = tool.get_blogs_by_category_and_page(categoryId, page, size, sortField, sortOrder);
                    totalCount = tool.get_total_blog_count_by_category(categoryId);
                } else {
                    // 普通查询
                    cout << "444" << endl;
                    blogs = tool.get_blogs_by_page_and_sort(page, size, sortField, sortOrder);
                    totalCount = tool.get_total_blog_count();
                }
            }

            // 构造 JSON 响应
            jsonData = "{";
            jsonData += "\"posts\": [";
            for (int i = 0; i < blogs.size(); i++) {
                Blog blog = blogs[i];
                string categoryName = tool.get_cotegoriename_by_cotegorieid(blog.get_category_id());
                User user = tool.get_userdata_by_userid(blog.get_user_id());
                string escapedTitle = escapeJsonString(blog.get_blog_title());
                string escapedContent = escapeJsonString(blog.get_blog_content());

                jsonData += "{";
                jsonData += "\"id\": \"" + std::to_string(blog.get_blog_id()) + "\",";
                jsonData += "\"title\": \"" + escapedTitle + "\",";
                jsonData += "\"user\": \"" + user.get_username() + "\",";
                jsonData += "\"categoryId\": \"" + std::to_string(blog.get_category_id()) + "\",";
                jsonData += "\"categoryName\": \"" + categoryName + "\",";
                jsonData += "\"createdAt\": \"" + blog.get_blog_postTime() + "\",";
                jsonData += "\"updatedAt\": \"" + blog.get_updatedAt() + "\",";
                jsonData += "\"views\": \"" + std::to_string(blog.get_views()) + "\"";
                jsonData += "}";
                if (i < blogs.size() - 1) jsonData += ",";
            }
            jsonData += "],";
            int totalPages = (totalCount / 10) + 1;
            jsonData += "\"totalPages\": " + std::to_string(totalPages) + ",";
            jsonData += "\"currentPage\": " + std::to_string(page) + ",";
            jsonData += "\"totalPosts\": " + std::to_string(totalCount);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-文章管理-获取单个文章详情
    else if(m_method == GET && strstr(m_url, "/admins/posts/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            // 提取博客 ID
            char* postIdParam = strstr(m_url, "/admins/posts/") + 14; // 跳过 "/admins/posts/"
            if (postIdParam) {
                int blogId = std::atoi(postIdParam); // 将字符串转换为整数
                if (blogId > 0) {
                    // 根据博客 ID 查询数据库
                    sql_blog_tool tool;
                    Blog blog = tool.select_blog_by_id(blogId);
                    string escapedTitle = escapeJsonString(blog.get_blog_title());
                    string escapedContent = escapeJsonString(blog.get_blog_content());
                    // 构造 JSON 响应
                    jsonData = "{";
                    jsonData += "\"id\": " + std::to_string(blog.get_blog_id()) + ",";
                    jsonData += "\"title\": \"" + escapedTitle + "\",";
                    jsonData += "\"categoryId\": \"" + std::to_string(blog.get_category_id()) + "\",";
                    jsonData += "\"content\": \"" + escapedContent + "\",";
                    // jsonData += "\"excerpt\": \"" + string("文章摘要...") + "\",";
                    // jsonData += "\"status\": \"" + string("published") + "\",";

                    // 处理 tags 数组
                    vector<string> tags = tool.get_tags_by_blogid(blogId); // 假设此方法返回 vector<string>
                    jsonData += "\"tags\": [";
                    for (size_t i = 0; i < tags.size(); i++) {
                        jsonData += "\"" + tags[i] + "\"";
                        if (i < tags.size() - 1) jsonData += ",";
                    }
                    jsonData += "],";

                    jsonData += "\"createdAt\": \"" + blog.get_blog_postTime() + "\",";
                    jsonData += "\"updatedAt\": \"" + blog.get_updatedAt() + "\",";
                    jsonData += "\"views\": " + string("100");
                    jsonData += "}";

                    return BLOG_DATA;
                }else{
                    return BAD_REQUEST; // 如果 blogId 无效
                }
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // // 管理员-文章管理-更新文章
    // else if(m_method == PATCH && strstr(m_url, "/admins/posts/")){
    //     string username = cookie_admin.getCookie("username");
    //     string session_id = cookie_admin.getCookie("session_id");

    //     if(cookie_admin.validateSession(username, session_id)){
    //         sql_blog_tool tool;
    //         // 提取文章ID
    //         char* postIdParam = strstr(m_url, "/admins/posts/") + 14; // 跳过 "/admins/posts/"
    //         int postId = std::atoi(postIdParam);
    //         Blog post = tool.select_blog_by_id(postId);

    //         // 解析JSON格式的请求体
    //         json requestJson = json::parse(m_string);
            
    //         string title = requestJson["title"];
    //         string categoryId = requestJson["categoryId"];
    //         string content = requestJson["content"];
            
    //         // 处理标签数组
    //         vector<string> tagsList;
    //         if(requestJson.contains("tags") && requestJson["tags"].is_array()) {
    //             for(const auto& tag : requestJson["tags"]) {
    //                 tagsList.push_back(tag);
    //             }
    //         }
            
    //         // 校验文章数据
    //         if (title.empty() || content.empty()) {
    //             return BAD_REQUEST;
    //         }

    //         Blog updatedPost;
    //         updatedPost.set_blog_id(postId);
    //         updatedPost.set_blog_title(title);
    //         updatedPost.set_category_id(std::atoi(categoryId.c_str()));
    //         updatedPost.set_blog_content(content);
            
    //         // 更新文章
    //         if(tool.update_blog_by_blogid(postId, updatedPost)){
    //             // 更新标签关联
    //             tool.delete_blog_tags(postId); // 先删除旧标签关联
    //             for(const auto& tag : tagsList) {
    //                 // 如果标签不存在，先创建标签
    //                 int tagId = tool.get_tag_id_by_name(tag);
    //                 if(tagId == -1) {
    //                     tagId = tool.create_tag(tag);
    //                 }
    //                 if(tagId != -1) {
    //                     tool.add_blog_tag(postId, tagId);
    //                 }
    //             }
                
    //             Blog newPost = tool.select_blog_by_id(postId);
    //             jsonData = "{";
    //             jsonData += "\"id\":\"" + to_string(newPost.get_blog_id()) + "\",";
    //             jsonData += "\"title\":\"" + newPost.get_blog_title() + "\"";
    //             jsonData += "}";
    //             return BLOG_DATA;
    //         }
    //         return BAD_REQUEST;
    //     }
    //     else{
    //         return BAD_REQUEST;
    //     }
    // }
    // 管理员-文章管理-删除文章
    else if(m_method == DELETE && strstr(m_url, "/admins/posts")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            char* postIdParam = strstr(m_url, "/admins/posts/") + 14; // 跳过 "/admins/posts/"
            if (postIdParam) {
                int blogId = std::atoi(postIdParam); // 将字符串转换为整数
                if (blogId > 0) {
                    sql_blog_tool tool;
                    tool.delete_blog_by_blogid(blogId);
                    jsonData = "{";
                    jsonData += "\"message\":" + string("文章删除成功");
                    jsonData += "}";
                }
            }
            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-文章管理-获取文章分类列表
    else if(strstr(m_url, "/admins/blogcategories")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            // 查询数据库
            sql_blog_tool tool;
            vector<Categories> categories = tool.get_cotegories_all();

            // 构造 JSON 响应
            jsonData += "[";
            for (int i = 0; i < categories.size(); i++) {
                Categories categorie = categories[i];
                jsonData += "{";
                jsonData += "\"id\": " + std::to_string(categorie.get_id()) + ",";
                jsonData += "\"name\": \"" + categorie.get_name() + "\"";
                jsonData += "}";
                if (i < categories.size() - 1) jsonData += ",";
            }
            jsonData += "]";
            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-评论管理-获取评论列表（分页、搜索、筛选）
    else if(m_method == GET && strstr(m_url, "/admins/comments?")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            int page = 1, size = 10;
            string searchKeyword = ""; // 默认无搜索
            string sortField = "created_at";
            string sortOrder = "desc";

            // 解析url参数
            char* pageParam = strstr(m_url, "page=");
            char* sizeParam = strstr(m_url, "pageSize=");
            char* searchParam = strstr(m_url, "search=");
            char* sortParam = strstr(m_url, "sort=");

            if (pageParam) {
                page = std::atoi(pageParam + 5);
                if (page <= 0) page = 1;
            }
            if (sizeParam) {
                size = std::atoi(sizeParam + 9);
                if (size <= 0 || size > 100) size = 20;
            }
            if (searchParam) {
                searchKeyword = string(searchParam + 7);
                size_t ampPos = searchKeyword.find("&");
                if (ampPos != string::npos) {
                    searchKeyword = searchKeyword.substr(0, ampPos);
                }
                searchKeyword = url_decode(searchKeyword);  // 进行 URL 解码
            }
            if (sortParam) {
                string sortStr = string(sortParam + 5);
                size_t pos = sortStr.rfind("_");  // 改为 rfind("_")，确保取最后一个 "_"

                if (pos != string::npos) {
                    sortField = sortStr.substr(0, pos);
                    sortOrder = sortStr.substr(pos + 1);

                    // 防止将后续的 &categoryId=1 作为排序顺序的一部分
                    size_t ampPos = sortOrder.find("&");
                    if (ampPos != string::npos) {
                        sortOrder = sortOrder.substr(0, ampPos);  // 只取排序方向部分
                    }
                }
            }
            
            // 查询数据库
            sql_blog_tool tool;
            vector<Comments> comments;
            int totalCount = 0;

            if (!searchKeyword.empty()) {
                comments = tool.get_comments_by_page_and_sort_and_search(page, size, sortField, sortOrder, searchKeyword);
                totalCount = tool.get_total_comments_count_by_search(searchKeyword);
            } else {
                comments = tool.get_comments_by_page_and_sort(page, size, sortField, sortOrder);
                totalCount = tool.get_comment_count();
            }

            // 构造 JSON 响应
            jsonData = "{";
            jsonData += "\"comments\": [";
            for (int i = 0; i < comments.size(); i++) {
                Comments comment = comments[i];
                Blog blog = tool.select_blog_by_id(comment.get_blog_id());
                string escapedUser = escapeJsonString(comment.get_username());
                string escapedContent = escapeJsonString(comment.get_content());
                

                jsonData += "{";
                jsonData += "\"id\": " + std::to_string(comment.get_comment_id()) + ",";
                jsonData += "\"author\": \"" + escapedUser + "\",";
                // jsonData += "\"email\": \"" + user.get_eamil() + "\",";
                jsonData += "\"content\": \"" + comment.get_content() + "\",";
                jsonData += "\"articleId\": " + std::to_string(comment.get_blog_id()) + ",";
                jsonData += "\"articleTitle\": \"" + blog.get_blog_title() + "\",";
                // jsonData += "\"status\": \"" + std::string("approved") + "\",";
                jsonData += "\"createdAt\": \"" + comment.get_comment_time() + "\"";
                jsonData += "}";
                if (i < comments.size() - 1) jsonData += ",";
            }
            jsonData += "],";
            int totalPages = (totalCount + size - 1) / size; // 使用整数除法向上取整的技巧
            jsonData += "\"totalPages\": " + std::to_string(totalPages);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-评论管理-获取单个评论详情
    // else if(m_method == GET && strstr(m_url, "/admins/comments")){
    //     string username = cookie_admin.getCookie("username");
    //     string session_id = cookie_admin.getCookie("session_id");

    //     if(cookie_admin.validateSession(username, session_id)){
    //         char* commentIdParam = strstr(m_url, "/admins/comments/") + 17; // 跳过 "/admins/posts/"
    //         if (commentIdParam) {
    //             int commentId = std::atoi(commentIdParam); // 将字符串转换为整数
    //             if (commentId > 0) {
    //                 sql_blog_tool tool;
    //                 Comments comment = tool.get_comment_by_commentId(commentId);
    //                 Blog blog = tool.select_blog_by_id(comment.get_blog_id());
    //                 string escapedUser = escapeJsonString(comment.get_username());
    //                 string escapedContent = escapeJsonString(comment.get_content());

    //                 jsonData += "{";
    //                 jsonData += "\"id\": " + std::to_string(comment.get_comment_id()) + ",";
    //                 jsonData += "\"author\": \"" + escapedUser + "\",";
    //                 jsonData += "\"email\": \"" + string("1111@example.com") + "\",";
    //                 jsonData += "\"content\": \"" + escapedContent + "\",";
    //                 jsonData += "\"articleId\": " + std::to_string(comment.get_blog_id()) + ",";
    //                 jsonData += "\"articleTitle\": \"" + blog.get_blog_title() + "\",";
    //                 // jsonData += "\"status\": \"" + std::string("approved") + "\",";
    //                 jsonData += "\"createdAt\": \"" + comment.get_comment_time() + "\"";
    //                 jsonData += "}";

    //                 return BLOG_DATA;
    //             }else{
    //                 return BAD_REQUEST;
    //             }
    //         }
    //         return BLOG_DATA;
    //     }else{
    //         return BAD_REQUEST;
    //     }
    // }
    // 管理员-评论管理-更新评论
    // else if(m_method == PATCH && strstr(m_url, "/admins/comments")){
    //     string username = cookie_admin.getCookie("username");
    //     string session_id = cookie_admin.getCookie("session_id");

    //     if(cookie_admin.validateSession(username, session_id)){
    //         char* commentIdParam = strstr(m_url, "/admins/comments/") + 17; // 跳过 "/admins/posts/"
    //         if (commentIdParam) {
    //             int commentId = std::atoi(commentIdParam); // 将字符串转换为整数
    //             if (commentId > 0) {
    //                 auto post_data = parse_post_data(m_string);
    //                 string newcontent = post_data["content"];
    //                 string newstatus = post_data["status"];
    //                 Comments comment;
    //                 comment.set_content(newcontent);
                    

    //                 sql_blog_tool tool;
    //                 if(tool.update_comment_by_commentid(commentId, comment)){
    //                     jsonData += "{";
    //                     jsonData += "\"success\": " + string("true") + ",";
    //                     jsonData += "\"message\": \"" + string("评论更新成功") + "\"";
    //                     jsonData += "}";

    //                     return BLOG_DATA;
    //                 }

    //             }else{
    //                 return BAD_REQUEST;
    //             }
    //         }
    //         return BLOG_DATA;
    //     }else{
    //         return BAD_REQUEST;
    //     }
    // }
    // 管理员-评论管理-删除单条评论
    else if(m_method == DELETE && strstr(m_url, "/admins/comments")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            char* commentIdParam = strstr(m_url, "/admins/comments/") + 17; // 跳过 "/admins/posts/"
            if (commentIdParam) {
                int commentId = std::atoi(commentIdParam); // 将字符串转换为整数
                if (commentId > 0) {
                    sql_blog_tool tool;
                    bool result = tool.delete_comment_by_commentid(commentId);
                    if(result){
                        jsonData = "{";
                        jsonData += "\"success\":" + string("true") + ",";
                        jsonData += "\"message\":\"" + string("评论已删除") + "\"";
                        jsonData += "}";
                    }else{
                        return BAD_REQUEST;
                    }
                }else{
                    return BAD_REQUEST;
                }
            }
            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-评论管理-批量删除评论
    // else if (m_method == POST && strstr(m_url, "/admins/comments/batch-delete")) {
    //     string username = cookie_admin.getCookie("username");
    //     string session_id = cookie_admin.getCookie("session_id");

    //     if (cookie_admin.validateSession(username, session_id)) {
    //         vector<int> commentIds;
            
    //         // 获取原始字符串指针
    //         char* input = m_string;
            
    //         // 查找ids数组起始位置
    //         char* array_start = strstr(input, "\"ids\"");
    //         if (!array_start) return BAD_REQUEST;
            
    //         array_start = strchr(array_start, '[');  // 定位到数组开始
    //         if (!array_start) return BAD_REQUEST;
            
    //         char* array_end = strchr(array_start, ']'); // 定位到数组结束
    //         if (!array_end) return BAD_REQUEST;

    //         // 遍历数组内容
    //         char* current = array_start + 1;
    //         while (current < array_end) {
    //             // 跳过空白字符
    //             while (current < array_end && isspace(*current)) ++current;
    //             if (current >= array_end) break;

    //             // 处理引号（支持带引号和不带引号的数字）
    //             char* value_start = current;
    //             if (*value_start == '"') {
    //                 ++value_start;  // 跳过开头的引号
    //                 char* value_end = strchr(value_start, '"');
    //                 if (!value_end || value_end > array_end) break;
    //                 current = value_end + 1;  // 移动到下一个元素
    //                 parse_id(value_start, value_end - value_start, commentIds);
    //             } else {
    //                 // 不带引号的数字
    //                 char* comma = strchr(current, ',');
    //                 char* value_end = (comma && comma < array_end) ? comma : array_end;
    //                 parse_id(current, value_end - current, commentIds);
    //                 current = value_end + 1;
    //             }

    //             // 跳过逗号
    //             while (current < array_end && (*current == ',' || isspace(*current))) ++current;
    //         }

    //         if (commentIds.empty()) return BAD_REQUEST;

    //         // 执行批量删除
    //         sql_blog_tool tool;
    //         int affectedCount = tool.batch_delete_comments(commentIds);

    //         // 构造响应（使用安全格式化）
    //         char buffer[256];
    //         snprintf(buffer, sizeof(buffer), 
    //             "{\"success\":true,\"message\":\"已成功删除%d条评论\",\"affectedCount\":%d}",
    //             affectedCount, affectedCount);
    //         jsonData = buffer;

    //         return BLOG_DATA;
    //     } else {
    //         return BAD_REQUEST;
    //     }
    // }
    // 管理员-用户管理-获取用户列表（分页、搜索、筛选）
    else if(m_method == GET && strstr(m_url, "/admins/users?")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            int page = 1, size = 10;
            string sortField = "register_time"; // 默认排序字段(关键词) 按注册时间、登录时间、名称、文章数
            string sortOrder = "desc"; // 默认排序方式 升序or降序
            string status = "";         // 默认全部状态  可使用、已封禁
            string searchKeyword = ""; // 默认无搜索 搜索用户名、邮箱

            // 解析url参数
            char* pageParam = strstr(m_url, "page=");
            char* sizeParam = strstr(m_url, "pageSize=");
            char* sortParam = strstr(m_url, "sort=");
            char* statusParam = strstr(m_url, "status=");
            char* searchParam = strstr(m_url, "search=");

            if (pageParam) {
                page = std::atoi(pageParam + 5);
                if (page <= 0) page = 1;
            }
            if (sizeParam) {
                size = std::atoi(sizeParam + 9);
                if (size <= 0 || size > 100) size = 20;
            }
            if (sortParam) {
                string sortStr = string(sortParam + 5);
                size_t pos = sortStr.rfind("_");  // 改为 rfind("_")，确保取最后一个 "_"

                if (pos != string::npos) {
                    sortField = sortStr.substr(0, pos);
                    sortOrder = sortStr.substr(pos + 1);

                    // 防止将后续的 &categoryId=1 作为排序顺序的一部分
                    size_t ampPos = sortOrder.find("&");
                    if (ampPos != string::npos) {
                        sortOrder = sortOrder.substr(0, ampPos);  // 只取排序方向部分
                    }
                }
            }
            if (statusParam) {
                status = string(statusParam + 7);
                size_t ampPos = status.find("&");
                if (ampPos != string::npos) {
                    status = status.substr(0, ampPos);
                }
            }
            if (searchParam) {
                searchKeyword = string(searchParam + 7);
                size_t ampPos = searchKeyword.find("&");
                if (ampPos != string::npos) {
                    searchKeyword = searchKeyword.substr(0, ampPos);
                }
                searchKeyword = url_decode(searchKeyword);  // 进行 URL 解码
            }
            
            // 查询数据库
            sql_blog_tool tool;
            vector<User> users;
            int totalCount = 0;

            if (searchKeyword.empty()) {
                if (status.empty()) {
                    // 分页查询+排序
                    users = tool.get_users_by_page_and_sort(page, size, sortField, sortOrder);
                    totalCount = tool.get_user_count();
                } else {
                    // 状态+分页查询+排序
                    users = tool.get_users_by_page_and_sort_and_status(page, size, sortField, sortOrder, status);
                    totalCount = tool.get_user_count_by_status(status);
                }
            } else {
                if (status.empty()) {
                    // 搜索+分页查询+排序
                    users = tool.get_users_by_page_and_sort_and_search(page, size, sortField, sortOrder, searchKeyword);
                    totalCount = tool.get_user_count_by_search(searchKeyword);
                } else {
                    // 状态+搜索+分页查询+排序
                    users = tool.get_users_by_page_and_sort_and_status_search(page, size, sortField, sortOrder, status, searchKeyword);
                    totalCount = tool.get_user_count_by_status_search(status, searchKeyword);
                }
            }

            // 构造 JSON 响应
            jsonData = "{";
            jsonData += "\"users\": [";
            for (int i = 0; i < users.size(); i++) {
                User user = users[i];

                jsonData += "{";
                jsonData += "\"id\": \"" + std::to_string(user.get_userid()) + "\",";
                jsonData += "\"username\": \"" + user.get_username() + "\",";
                jsonData += "\"email\": \"" + user.get_eamil() + "\",";
                jsonData += "\"status\": \"" + user.get_status() + "\",";
                jsonData += "\"createdAt\": \"" + user.get_register_time() + "\",";
                jsonData += "\"lastLogin\": \"" + user.get_last_login_time() + "\",";
                jsonData += "\"avatar\": \"" + string("") + "\",";
                jsonData += "\"postsCount\": \"" + std::to_string(user.get_article_count()) + "\"";
                jsonData += "}";
                if (i < users.size() - 1) jsonData += ",";
            }
            jsonData += "],";
            int totalPages = (totalCount / 10) + 1;
            jsonData += "\"totalPages\": " + std::to_string(totalPages) + ",";
            jsonData += "\"currentPage\": " + std::to_string(page) + ",";
            jsonData += "\"totalUsers\": " + std::to_string(totalCount);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-用户管理-获取用户详情
    else if(m_method == GET && strstr(m_url, "/admins/users/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            // 提取博客 ID
            char* userIdParam = strstr(m_url, "/admins/users/") + 14; // 跳过 "/admins/users/"
            if (userIdParam) {
                int userId = std::atoi(userIdParam); // 将字符串转换为整数
                if (userId > 0) {
                    // 根据博客 ID 查询数据库
                    sql_blog_tool tool;
                    User user = tool.get_userdata_by_userid(userId);
                    string escapedBio = escapeJsonString(user.get_description());
                    // 构造 JSON 响应
                    jsonData = "{";
                    jsonData += "\"id\": " + std::to_string(user.get_userid()) + ",";
                    jsonData += "\"username\": \"" + user.get_username() + "\",";
                    jsonData += "\"email\": \"" + user.get_eamil() + "\",";
                    jsonData += "\"status\": \"" + user.get_status() + "\",";
                    jsonData += "\"bio\": \"" + escapedBio + "\",";
                    jsonData += "\"createdAt\": \"" + user.get_register_time() + "\",";
                    jsonData += "\"lastLogin\": \"" + user.get_register_time() + "\"";
                    jsonData += "}";

                    return BLOG_DATA;
                }else{
                    return BAD_REQUEST; // 如果 userId 无效
                }
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-用户管理-添加新用户
    else if(m_method == POST && strstr(m_url, "/admins/users")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            auto post_data = parse_post_data(m_string);
            string username = post_data["username"];
            string password = post_data["password"];
            string email = post_data["email"];
            string status = post_data["status"];
            string bio = post_data["bio"];

            if (username.empty()) return BAD_REQUEST;
            if(password.empty()) return BAD_REQUEST;
            if(status.empty()) return BAD_REQUEST;

            sql_blog_tool tool;
            if(tool.check_username_is_exist(username)){
                jsonData = "{\"exists\": true}";
                return BAD_REQUEST;
            }
            // 校验用户名
            if (!is_valid_username(username.c_str()))
            {
                return BAD_REQUEST;
            }

            // 校验密码
            if (!is_valid_password(password.c_str()))
            {
                return BAD_REQUEST;
            }

            // 生成 bcrypt 哈希
            std::string hashed_password = BCrypt::generateHash(password);

            User user;
            user.set_username(username);
            user.set_password(hashed_password);
            user.set_email(email);
            user.set_status(status);
            user.set_description(bio);

            // 将该用户插入用户表
            if(tool.add_user_from_admin(user)){
                users.insert(pair<string, string>(username, hashed_password)); // 数据插入成功后，存入users中
                jsonData = "{";
                        jsonData += "\"success\":" + string("true") + ",";
                        jsonData += "\"message\":\"" + string("用户创建成功") + "\",";
                        jsonData += "\"userId\":\"" + to_string(user.get_userid()) + "\"";
                        jsonData += "}";
                return BLOG_DATA;
            }

            return BLOG_DATA;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-用户管理-更新用户
    else if(m_method == PATCH && strstr(m_url, "/admins/users/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 提取用户数据  
            char* userIdParam = strstr(m_url, "/admins/users/") + 14; // 跳过 "/admins/users/"
            int userId = std::atoi(userIdParam);
            User olduser = tool.get_userdata_by_userid(userId);

            auto post_data = parse_post_data(m_string);
            string newusername = post_data["username"];
            string newpassword = post_data["password"];
            string email = post_data["email"];
            string status = post_data["status"];
            string bio = post_data["bio"];
            
            if(!newusername.empty() && tool.check_username_is_exist(newusername)){
                jsonData = "{\"exists\": true}";
                return BAD_REQUEST;
            }
            // 校验用户名
            if (!newusername.empty() && !is_valid_username(newusername.c_str()))
            {
                return BAD_REQUEST;
            }
            // 校验密码
            if (!newpassword.empty() && !is_valid_password(newpassword.c_str()))
            {
                return BAD_REQUEST;
            }

            // 生成 bcrypt 哈希
            std::string hashed_password;
            if(!newpassword.empty()) hashed_password = BCrypt::generateHash(newpassword);

            User user;
            user.set_username(newusername);
            user.set_password(hashed_password);
            user.set_email(email);
            user.set_status(status);
            user.set_description(bio);

            // 将该用户插入用户表
            if(tool.update_user_by_userid(userId, user)){
                auto it = users.find(olduser.get_username());
                if (it != users.end()) {
                    std::string oldpassword = it->second; // 旧密码

                    // 如果 password 变更，则更新
                    if (!newpassword.empty() && newpassword != oldpassword) {
                        it->second = newpassword;
                    }

                    // 如果 username 变更，则删除旧的键值对，插入新的
                    if (!newusername.empty() && newusername != olduser.get_username()) {
                        users.erase(it);
                        users.insert({newusername, newpassword}); // 保持旧密码
                    }

                    // 如果都变，则删除旧的键值对，插入新的
                    if (!newusername.empty() && newusername != olduser.get_username() && !newpassword.empty() && newpassword != olduser.get_password()) {
                        users.erase(it);
                        users.insert({newusername, newpassword}); // 保持旧密码
                    }
                } else {
                    // 如果旧用户名不存在
                    return BAD_REQUEST;
                }

                User newUser = tool.get_userdata_by_userid(userId);
                jsonData = "{";
                        jsonData += "\"message\":\"" + string("用户更新成功") + "\",";
                        jsonData += "\"user\": " + 
                            jsonData += "\"username\":\"" + newUser.get_username() + "\",";
                            jsonData += "\"email\":\"" + newUser.get_eamil() + "\",";
                            jsonData += "\"status\":\"" + newUser.get_status() + "\",";
                            jsonData += "\"bio\":\"" + newUser.get_description() + "\"";
                         + "}";
                        jsonData += "}";
                        jsonData += "}";
                return BLOG_DATA;
            }
            return BLOG_DATA;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-用户管理-删除指定用户
    else if(m_method == DELETE && strstr(m_url, "/admins/users/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            // 提取博客 ID
            char* userIdParam = strstr(m_url, "/admins/users/") + 14; // 跳过 "/admins/users/"
            if (userIdParam) {
                int userId = std::atoi(userIdParam); // 将字符串转换为整数
                if (userId > 0) {
                    // 根据博客 ID 查询数据库
                    sql_blog_tool tool;
                    bool result = tool.delete_user_by_userid(userId);
                    if(result) return BLOG_DATA;
                    return BAD_REQUEST;
                }else{
                    return BAD_REQUEST; // 如果 userId 无效
                }
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-分类管理-获取分类列表（分页、搜索、筛选）
    else if(m_method == GET && strstr(m_url, "/admins/categories?")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            int page = 1, size = 10;
            string sortField = "created_at"; // 默认排序字段(关键词) 按创建时间、名称、文章数
            string sortOrder = "desc"; // 默认排序方式 升序or降序
            string searchKeyword = ""; // 默认无搜索 搜索用户名、邮箱

            // 解析url参数
            char* pageParam = strstr(m_url, "page=");
            char* sizeParam = strstr(m_url, "pageSize=");
            char* sortParam = strstr(m_url, "sort=");
            char* searchParam = strstr(m_url, "search=");

            if (pageParam) {
                page = std::atoi(pageParam + 5);
                if (page <= 0) page = 1;
            }
            if (sizeParam) {
                size = std::atoi(sizeParam + 9);
                if (size <= 0 || size > 100) size = 20;
            }
            if (sortParam) {
                string sortStr = string(sortParam + 5);
                size_t pos = sortStr.rfind("_");  // 改为 rfind("_")，确保取最后一个 "_"

                if (pos != string::npos) {
                    sortField = sortStr.substr(0, pos);
                    sortOrder = sortStr.substr(pos + 1);

                    size_t ampPos = sortOrder.find("&");
                    if (ampPos != string::npos) {
                        sortOrder = sortOrder.substr(0, ampPos);  // 只取排序方向部分
                    }
                }
            }
            if (searchParam) {
                searchKeyword = string(searchParam + 7);
                size_t ampPos = searchKeyword.find("&");
                if (ampPos != string::npos) {
                    searchKeyword = searchKeyword.substr(0, ampPos);
                }
                searchKeyword = url_decode(searchKeyword);  // 进行 URL 解码
            }
            
            // 查询数据库
            sql_blog_tool tool;
            vector<Categories> categories;
            int totalCount = 0;

            if(sortField == "article_count") {
                // 使用特殊的排序逻辑，按博客数量排序
                if(searchKeyword.empty()) {
                    categories = tool.get_categories_by_articles_count(page, size, sortOrder);
                    totalCount = tool.get_categorie_count();
                } else {
                    categories = tool.get_categories_by_articles_count_and_search(page, size, sortOrder, searchKeyword);
                    totalCount = tool.get_total_categories_count_by_search(searchKeyword);
                }
            } else if(searchKeyword.empty()) {
                // 其他排序逻辑保持不变
                categories = tool.get_categories_by_page_and_sort(page, size, sortField, sortOrder);
                totalCount = tool.get_categorie_count();
            } else {
                // 其他排序逻辑保持不变
                categories = tool.get_categories_by_page_and_sort_and_search(page, size, sortField, sortOrder, searchKeyword);
                totalCount = tool.get_total_categories_count_by_search(searchKeyword);
            }

            // 构造 JSON 响应
            jsonData = "{";
            jsonData += "\"categories\": [";
            for (int i = 0; i < categories.size(); i++) {
                Categories categorie = categories[i];
                int searchId = categorie.get_id();
                int articlesCount = tool.get_total_blog_count_by_category(searchId);

                jsonData += "{";
                jsonData += "\"id\": \"" + std::to_string(categorie.get_id()) + "\",";
                jsonData += "\"name\": \"" + categorie.get_name() + "\",";
                jsonData += "\"description\": \"" + categorie.get_description() + "\",";
                jsonData += "\"icon\": \"" + string("") + "\",";
                jsonData += "\"createdAt\": \"" + categorie.get_created_at() + "\",";
                jsonData += "\"articlesCount\": \"" + std::to_string(articlesCount) + "\"";
                jsonData += "}";
                if (i < categories.size() - 1) jsonData += ",";
            }
            jsonData += "],";
            int totalPages = (totalCount / 10) + 1;
            jsonData += "\"totalPages\": " + std::to_string(totalPages) + ",";
            // jsonData += "\"currentPage\": " + std::to_string(page) + ",";
            jsonData += "\"totalCount\": " + std::to_string(totalCount);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-分类管理-获取分类详情
    else if(m_method == GET && strstr(m_url, "/admins/categories/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            // 提取博客 ID
            char* categorieIdParam = strstr(m_url, "/admins/categories/") + 19;
            if (categorieIdParam) {
                int categorieId = std::atoi(categorieIdParam); // 将字符串转换为整数
                if (categorieId > 0) {
                    // 根据博客 ID 查询数据库
                    sql_blog_tool tool;
                    Categories categorie = tool.get_categorie_by_categorieid(categorieId);
                    int searchId = categorie.get_id();
                    int articlesCount = tool.get_total_blog_count_by_category(searchId);

                    // 构造 JSON 响应
                    jsonData = "{";
                    jsonData += "\"id\": " + std::to_string(categorie.get_id()) + ",";
                    jsonData += "\"name\": \"" + categorie.get_name() + "\",";
                    jsonData += "\"description\": \"" + categorie.get_description() + "\",";
                    jsonData += "\"icon\": \"" + string("") + "\",";
                    jsonData += "\"createdAt\": \"" + categorie.get_created_at() + "\",";
                    jsonData += "\"updatedAt\": \"" + categorie.get_updated_at() + "\",";
                    jsonData += "\"articlesCount\": \"" + to_string(articlesCount) + "\"";
                    jsonData += "}";

                    return BLOG_DATA;
                }else{
                    return BAD_REQUEST; // 如果 userId 无效
                }
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-分类管理-添加新分类
    else if(m_method == POST && strstr(m_url, "/admins/categories")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            auto post_data = parse_post_data(m_string);
            string name = post_data["name"];
            string description = post_data["description"];

            if (name.empty()) return BAD_REQUEST;

            sql_blog_tool tool;
            Categories categorie;
            categorie.set_name(name);
            categorie.set_description(description);

            if(tool.add_categorie(categorie)){
                Categories category = tool.get_categorie_by_name(name);
                jsonData = "{";
                        jsonData += "\"id\":\"" + to_string(category.get_id()) + "\",";
                        jsonData += "\"name\":\"" + category.get_name() + "\",";
                        jsonData += "\"description\":\"" + category.get_description() + "\",";
                        jsonData += "\"createdAt\":\"" + category.get_created_at() + "\",";
                        jsonData += "\"updatedAt\":\"" + category.get_updated_at() + "\"";
                        jsonData += "}";
                return BLOG_DATA;
            }

            return BLOG_DATA;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-分类管理-更新分类
    else if(m_method == PATCH && strstr(m_url, "/admins/categories/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            sql_blog_tool tool;
            // 提取用户数据  
            char* categorieIdParam = strstr(m_url, "/admins/categories/") + 19; // 跳过 "/admins/categories/"
            int categorieId = std::atoi(categorieIdParam);

            auto post_data = parse_post_data(m_string);
            string newname = post_data["name"];
            string newdescription = post_data["description"];

            Categories categorie;
            if(!newname.empty()) {
                categorie.set_name(newname);
            }
            if(!newdescription.empty()) {
                categorie.set_description(newdescription);
            }

            if(tool.update_categorie_by_categorieid(categorieId, categorie)){
                Categories newcategorie = tool.get_categorie_by_categorieid(categorieId);
                jsonData = "{";
                        jsonData += "\"id\":\"" + to_string(newcategorie.get_id()) + "\",";
                        jsonData += "\"name\":\"" + newcategorie.get_name() + "\",";
                        jsonData += "\"description\":\"" + newcategorie.get_description() + "\",";
                        jsonData += "\"updatedAt\":\"" + newcategorie.get_updated_at() + "\"";
                        jsonData += "}";
                return BLOG_DATA;
            }
            return BLOG_DATA;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-分类管理-删除指定分类
    else if(m_method == DELETE && strstr(m_url, "/admins/categories/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){   
            // 提取博客 ID
            char* categorieIdParam = strstr(m_url, "/admins/categories/") + 19; // 跳过 "/admins/categories/"
            if (categorieIdParam) {
                int categorieId = std::atoi(categorieIdParam); // 将字符串转换为整数
                if (categorieId > 0) {
                    // 根据博客 ID 查询数据库
                    sql_blog_tool tool;
                    bool result = tool.delete_categorie_by_categorieid(categorieId);
                    if(result){
                        jsonData = "{";
                        jsonData = "\"message\": \"" + string("分类删除成功") + "\",";
                        jsonData = "\"deletedCategoryId\": \"" + to_string(categorieId) + "\"";
                        jsonData = "}";
                        return BLOG_DATA;
                    }
                    return BAD_REQUEST;
                }else{
                    return BAD_REQUEST; // 如果 userId 无效
                }
            }
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-标签管理-获取标签列表（分页、搜索、筛选）
    else if(m_method == GET && strstr(m_url, "/admins/tags?")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            int page = 1, size = 10;
            string sortField = "created_at"; // 默认排序字段(关键词) 按创建时间、名称、文章数
            string sortOrder = "desc"; // 默认排序方式 升序or降序
            string searchKeyword = ""; // 默认无搜索 搜索用户名、邮箱

            // 解析url参数
            char* pageParam = strstr(m_url, "page=");
            char* sizeParam = strstr(m_url, "pageSize=");
            char* sortParam = strstr(m_url, "sort=");
            char* searchParam = strstr(m_url, "search=");

            if(pageParam){
                page = std::atoi(pageParam + 5);
                if(page <= 0) page = 1;
            }
            if(sizeParam){
                size = std::atoi(sizeParam + 9);
                if(size <= 0 || size > 100) size = 20;
            }
            if(sortParam){
                string sortStr = string(sortParam + 5);
                size_t pos = sortStr.rfind("_");
                if(pos != string::npos){
                    sortField = sortStr.substr(0, pos);
                    sortOrder = sortStr.substr(pos + 1);

                    size_t ampPos = sortOrder.find("&");
                    if(ampPos != string::npos){
                        sortOrder = sortOrder.substr(0, ampPos);
                    }
                }
            }
            if(searchParam){
                searchKeyword = string(searchParam + 7);
                size_t ampPos = searchKeyword.find("&");
                if(ampPos != string::npos){
                    searchKeyword = searchKeyword.substr(0, ampPos);
                }
                searchKeyword = url_decode(searchKeyword);
            }

            // 查询数据库
            sql_blog_tool tool;
            vector<Tags> tags;
            int totalCount = 0;

            if(sortField == "article_count"){
                if(searchKeyword.empty()){
                    tags = tool.get_tags_by_blog_count(page, size, sortOrder);
                    totalCount = tool.get_tag_count();
                }else{
                    tags = tool.get_tags_by_blog_count_and_search(page, size, sortOrder, searchKeyword);
                    totalCount = tool.get_total_tags_count_by_search(searchKeyword);
                }
            }else if(searchKeyword.empty()){
                tags = tool.get_tags_by_page_and_sort(page, size, sortField, sortOrder);
                totalCount = tool.get_tag_count();
            }else{
                tags = tool.get_tags_by_page_and_sort_and_search(page, size, sortField, sortOrder, searchKeyword);
                totalCount = tool.get_total_tags_count_by_search(searchKeyword);
            }

            // 构造JSON响应
            jsonData = "{";
            jsonData += "\"tags\": [";
            for(int i = 0; i < tags.size(); i++){
                Tags tag = tags[i];
                int searchId = tag.get_id();
                int blogCount = tool.get_total_blog_count_by_tag(searchId);

                jsonData += "{";
                jsonData += "\"id\": \"" + to_string(tag.get_id()) + "\",";
                jsonData += "\"name\": \"" + tag.get_name() + "\",";
                jsonData += "\"description\": \"" + tag.get_description() + "\",";
                jsonData += "\"createdAt\": \"" + tag.get_created_at() + "\",";
                jsonData += "\"blogCount\": \"" + to_string(blogCount) + "\"";
                jsonData += "}";
                if(i < tags.size() - 1) jsonData += ",";
            }
            jsonData += "],";
            int totalPages = (totalCount / 10) + 1;
            jsonData += "\"totalPages\": " + to_string(totalPages) + ",";
            jsonData += "\"totalCount\": " + to_string(totalCount);
            jsonData += "}";

            return BLOG_DATA;
        }else{
            return BAD_REQUEST;
        }
    }
    // 管理员-标签管理-添加新标签
    else if(m_method == POST && strstr(m_url, "/admins/tags")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            auto post_data = parse_post_data(m_string);
            string name = post_data["name"];
            string description = post_data["description"];

            if(name.empty()) return BAD_REQUEST;

            sql_blog_tool tool;
            Tags tag;
            tag.set_name(name);
            tag.set_description(description);

            if(tool.add_tag(tag)){
                Tags newtag = tool.get_tag_by_tagname(name);
                jsonData = "{";
                        jsonData += "\"id\":\"" + to_string(newtag.get_id()) + "\",";
                        jsonData += "\"name\":\"" + newtag.get_name() + "\",";
                        jsonData += "\"description\":\"" + newtag.get_description() + "\",";
                        jsonData += "\"createdAt\":\"" + newtag.get_created_at() + "\"";
                        jsonData += "}";
                return BLOG_DATA;
            }
            return BAD_REQUEST;
        }
        else{
            return BAD_REQUEST;
        }
    }
    // 管理员-标签管理-删除指定标签
    else if(m_method == DELETE && strstr(m_url, "/admins/tags/")){
        string username = cookie_admin.getCookie("username");
        string session_id = cookie_admin.getCookie("session_id");

        if(cookie_admin.validateSession(username, session_id)){
            // 提取标签ID
            char* tagIdParam = strstr(m_url, "/admins/tags/") + 13;
            if(tagIdParam){
                int tagId = std::atoi(tagIdParam);
                if(tagId > 0){
                    sql_blog_tool tool;
                    bool result = tool.delete_tag_by_tagid(tagId);
                    if(result){
                        jsonData = "{";
                        jsonData += "\"message\": \"" + string("标签删除成功") + "\"";
                        jsonData += "}";
                        return BLOG_DATA;
                    }
                    return BAD_REQUEST;
                }else{
                    return BAD_REQUEST;
                }
            }
            return BAD_REQUEST;
        }else{
            return BAD_REQUEST;
        }
    }
    else{
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }

    // 文件检查与映射
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;     // 插眼
    }
        

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
    case AUTHENTICATION:
    {
        add_status_line(401, error_401_title);
        add_response("Content-Type: application/json\r\n");
        add_headers(jsonData.length());
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = (void*)jsonData.c_str();
        m_iv[1].iov_len = jsonData.size();
        m_iv_count = 2; 
        bytes_to_send = m_write_idx + jsonData.length();
        return true;
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
    case REDIRECT_ADMIN:
    {
        add_status_line(302, ok_302_title);
        add_response("Location: /admins/admin_login.html\r\n");
        string cookie_header = cookie_admin.generateCookieHeaders();
        add_response(cookie_header.c_str());
        add_headers(strlen(ok_302_title));
    }
    case REDIRECT_HOME:
    {
        add_status_line(302, ok_302_title);
        add_response("Location: /blog_home.html\r\n");
        string cookie_header = cookie.generateCookieHeaders();
        add_response(cookie_header.c_str());
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
    case LOGIN:
    {
        add_status_line(200, ok_200_title);
        add_response("Content-Type: application/json\r\n");
        string cookie_headers = cookie.generateCookieHeaders();
        if(!cookie_headers.empty()){
            add_response(cookie_headers.c_str());
        }
        add_headers(jsonData.length());
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = (void*)jsonData.c_str();
        m_iv[1].iov_len = jsonData.size();
        m_iv_count = 2;
        bytes_to_send = m_write_idx + jsonData.length();
        return true;
    }
    case LOGOUT:
    {
        add_status_line(200, ok_200_title);
        add_response("Content-Type: application/json\r\n");
        string cookie_headers = cookie.generateCookieHeaders();
        if(!cookie_headers.empty()){
            add_response(cookie_headers.c_str());
        }
        add_headers(jsonData.length());
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = (void*)jsonData.c_str();
        m_iv[1].iov_len = jsonData.size();
        m_iv_count = 2;
        bytes_to_send = m_write_idx + jsonData.length();
        return true;

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
    case ADMIN_LOGIN_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)   // 若文件不为空
        {
            // 添加cookie响应头  
            string cookie_headers = cookie_admin.generateCookieHeaders();
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

void http_conn::process() // 在threadpool头文件里面调用
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
