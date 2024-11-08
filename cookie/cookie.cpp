#include "cookie.h"

// CookieOptions 构造函数：初始化默认值
Cookie::CookieOptions::CookieOptions() 
    : path("/"), expires(0), secure(false), httpOnly(false) {}

// Cookie 类的构造函数和析构函数
Cookie::Cookie() = default;  // 默认构造函数
Cookie::~Cookie() = default; // 默认析构函数

// 解析请求中的Cookie头部：将 Cookie 字符串解析为键值对，并存储在 request_cookies 中
void Cookie::parseCookieHeader(const string& header) {
    if (header.empty()) return;  // 如果头部为空，则返回

    stringstream ss(header);  // 使用 stringstream 来分割 Cookie 字符串
    string pair;

    // 逐个分割 cookie
    while (getline(ss, pair, ';')) {
        size_t pos = pair.find('=');
        if (pos != string::npos) {
            string key = trim(pair.substr(0, pos));      // 提取 Cookie 名称
            string value = trim(pair.substr(pos + 1));   // 提取 Cookie 值
            request_cookies[key] = value;                 // 存储到 request_cookies 映射中
        }
    }
}

// 设置要发送的 cookie：将 Cookie 设置到 response_cookies 列表中
void Cookie::setCookie(const string& name, const string& value, const CookieOptions& options) {
    CookieInfo cookie;
    cookie.name = name;
    cookie.value = value;
    cookie.options = options;
    response_cookies.push_back(cookie);  // 将 Cookie 添加到 response_cookies
}

// 获取请求中的 cookie 值：查找指定名称的 Cookie
string Cookie::getCookie(const string& name) const {
    auto it = request_cookies.find(name);  // 查找请求中是否有该 Cookie
    return (it != request_cookies.end()) ? it->second : "";  // 如果存在，返回值，否则返回空字符串
}

// 生成 Cookie 头部字符串：将要发送的 Cookies 格式化为 Set-Cookie 头部
string Cookie::generateCookieHeaders(){
    string headers;
    // 遍历所有要发送的 cookies
    for (const auto& cookie : response_cookies) {
        stringstream ss;
        ss << "Set-Cookie: " << cookie.name << "=" << cookie.value;

        // 根据 CookieOptions，添加各个属性到 Set-Cookie 字符串
        if (!cookie.options.path.empty()) {
            ss << "; Path=" << cookie.options.path;
        }
        if (!cookie.options.domain.empty()) {
            ss << "; Domain=" << cookie.options.domain;
        }
        if (cookie.options.expires > 0) {
            ss << "; Expires=" << formatTime(cookie.options.expires);  // 格式化时间
        }
        if (cookie.options.secure) {
            ss << "; Secure";  // 安全 Cookie
        }
        if (cookie.options.httpOnly) {
            ss << "; HttpOnly";  // HTTP-only Cookie
        }
        ss << "\r\n";  // 添加换行符

        headers += ss.str();  // 将每个 Cookie 的字符串拼接到 headers 中
    }

    // 清空response_cookies,避免重复发送
    response_cookies.clear();

    return headers;
}

// 会话管理方法：创建一个新的会话，并设置相应的 Cookie
string Cookie::createSession(const string& username) {
    // session_lock.lock();  // 锁定会话管理以保证线程安全

    // string session_id = generateSessionId();  // 生成会话 ID
    // active_sessions[username] = make_pair(session_id, time(nullptr));  // 记录会话

    // // 设置会话相关的 Cookie
    // CookieOptions options;
    // options.httpOnly = true;  // 设置 HttpOnly 属性，防止通过 JavaScript 访问
    // setCookie("session_id", session_id, options);
    // setCookie("username", username, options);
    // setCookie("isLogin", "true", options);  // 设置标志为已登录

    // session_lock.unlock();  // 解锁会话管理
    // return session_id;
    session_lock.lock();  // 锁定会话管理以保证线程安全

    // 检查用户是否已经有一个有效会话
    auto it = active_sessions.find(username);
    if (it != active_sessions.end()) {
        time_t current_time = time(nullptr);
        
        // 如果会话未过期，抛出异常或返回错误信息，禁止再次登录
        if (current_time - it->second.second <= SESSION_EXPIRE_TIME) {
            session_lock.unlock();  // 解锁
            throw runtime_error("User is already logged in.");  // 抛出异常表示用户已登录
        } else {
            // 如果会话已过期，则删除旧会话
            active_sessions.erase(it);
        }
    }

    // 创建新的会话
    string session_id = generateSessionId();
    active_sessions[username] = make_pair(session_id, time(nullptr));  // 记录新会话
    session_lock.unlock();  // 解锁会话管理

    // 设置会话相关的 Cookie
    CookieOptions options;
    options.httpOnly = true;
    setCookie("session_id", session_id, options);
    setCookie("username", username, options);
    setCookie("isLogin", "true", options);

    return session_id;    
}

// 会话验证方法：验证用户的会话是否有效
bool Cookie::validateSession(const string& username, const string& session_id) {
    session_lock.lock();  // 锁定会话管理

    auto it = active_sessions.find(username);  // 查找该用户名的会话
    if (it == active_sessions.end() || it->second.first != session_id) {
        session_lock.unlock();  // 如果会话无效，解锁并返回 false
        return false;
    }

    time_t current_time = time(nullptr);  // 获取当前时间
    // 如果会话超时，则删除会话并返回 false
    if (current_time - it->second.second > SESSION_EXPIRE_TIME) {
        active_sessions.erase(it);
        session_lock.unlock();
        return false;
    }

    // 更新会话的最后访问时间
    it->second.second = current_time;
    session_lock.unlock();  // 解锁会话管理
    return true;
}

// 移除会话：将用户的会话删除，并设置使相关 Cookie 过期
void Cookie::removeSession(const string& username) {
    session_lock.lock();  // 锁定会话管理
    active_sessions.erase(username);  // 删除会话
    session_lock.unlock();

    // 设置 Cookie 使其过期（通过设置过期时间为过去的时间）
    CookieOptions options;
    options.expires = time(nullptr) - 3600;  // 设置为过去的时间，强制过期
    setCookie("session_id", "", options);
    setCookie("username", "", options);
    setCookie("isLogin", "", options);
}

// 清理过期的会话：删除超过 24 小时未活跃的会话
void Cookie::cleanupSessions() {
    time_t current_time = time(nullptr);  // 获取当前时间
    session_lock.lock();  // 锁定会话管理
    for (auto it = active_sessions.begin(); it != active_sessions.end();) {
        // 如果会话超时，则删除
        if (current_time - it->second.second > 24 * 3600) {
            it = active_sessions.erase(it);
        } else {
            ++it;  // 否则，继续检查下一个会话
        }
    }
    session_lock.unlock();  // 解锁会话管理
}

// 辅助方法：去除字符串前后的空白字符
string Cookie::trim(const string& str) {
    size_t first = str.find_first_not_of(" \t");  // 查找第一个非空字符
    if (first == string::npos) return "";  // 如果没有找到，返回空字符串
    size_t last = str.find_last_not_of(" \t");  // 查找最后一个非空字符
    return str.substr(first, (last - first + 1));  // 返回去除空白后的字符串
}

// 生成会话 ID：生成一个 32 位的随机十六进制字符串作为会话 ID
string Cookie::generateSessionId() {
    random_device rd;  // 获取随机设备
    mt19937 gen(rd());  // 生成随机数引擎
    uniform_int_distribution<> dis(0, 15);  // 随机生成 0 到 15 之间的数
    const char* hex = "0123456789abcdef";  // 十六进制字符集

    string session_id;
    for (size_t i = 0; i < SESSION_ID_LENGTH; ++i) {
        session_id += hex[dis(gen)];  // 随机选择一个字符并加入到 session_id 中
    }
    return session_id;
}

// 格式化时间：将 time_t 类型的时间转化为符合 Cookie 规范的 GMT 格式字符串
string Cookie::formatTime(time_t t) {
    char buf[100];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));  // 使用 gmtime 获取 GMT 时间并格式化
    return string(buf);
}

// 静态成员初始化
unordered_map<string, pair<string, time_t>> Cookie::active_sessions;  // 存储所有活动会话
locker Cookie::session_lock;  // 用于保护会话管理的线程锁
