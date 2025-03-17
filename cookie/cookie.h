#ifndef COOKIE_H
#define COOKIE_H

#include <string>
#include <map>
#include <ctime>
#include <sstream>
#include <random>
#include <unordered_map>
#include "../lock/locker.h"

using namespace std;

class Cookie {
public:
    // Cookie属性结构
    struct CookieOptions {
        string path;
        string domain;
        time_t expires;
        bool secure;
        bool httpOnly;

        CookieOptions();
    };

    // 单个Cookie的完整信息
    struct CookieInfo {
        string name;
        string value;
        CookieOptions options;
    };

private:
    map<string, string> request_cookies;  // 存储请求中的cookies
    vector<CookieInfo> response_cookies;  // 存储要发送的cookies

    // 会话管理
    static unordered_map<string, pair<string, time_t>> active_sessions;  // username -> (session_id, timestamp) 
    static locker session_lock;

    // 会话配置
    const time_t SESSION_EXPIRE_TIME = 24 * 3600;  // 24小时
    const size_t SESSION_ID_LENGTH = 32;

public:
    Cookie();
    ~Cookie();

    // 解析请求中的Cookie头
    void parseCookieHeader(const string& header);

    // 设置要发送的cookie
    void setCookie(const string& name, const string& value, const CookieOptions& options = CookieOptions());

    // 获取请求中的cookie值
    string getCookie(const string& name) const;

    // 生成Cookie头部字符串
    string generateCookieHeaders();

    // 会话管理方法
    string createSession(const string& username);
    bool validateSession(const string& username, const string& session_id);
    void removeSession(const string& username);
    static void cleanupSessions();

private:
    // 辅助方法
    string trim(const string& str);
    string generateSessionId();
    static string formatTime(time_t t);
};




// 用来存储管理员的cookie
class Cookie_admin {
public:
    // Cookie属性结构
    struct CookieOptions {
        string path;
        string domain;
        time_t expires;
        bool secure;
        bool httpOnly;

        CookieOptions();
    };

    // 单个Cookie的完整信息
    struct CookieInfo {
        string name;
        string value;
        CookieOptions options;
    };

private:
    map<string, string> request_cookies;  // 存储请求中的cookies
    vector<CookieInfo> response_cookies;  // 存储要发送的cookies

    // 会话管理
    static unordered_map<string, pair<string, time_t>> active_sessions;  // username -> (session_id, timestamp) 
    static locker session_lock;

    // 会话配置
    const time_t SESSION_EXPIRE_TIME = 24 * 3600;  // 24小时
    const size_t SESSION_ID_LENGTH = 32;

public:
    Cookie_admin();
    ~Cookie_admin();

    // 解析请求中的Cookie头
    void parseCookieHeader(const string& header);

    // 设置要发送的cookie
    void setCookie(const string& name, const string& value, const CookieOptions& options = CookieOptions());

    // 获取请求中的cookie值
    string getCookie(const string& name) const;

    // 生成Cookie头部字符串
    string generateCookieHeaders();

    // 会话管理方法
    string createSession(const string& username);
    bool validateSession(const string& username, const string& session_id);
    void removeSession(const string& username);
    void forceClearClientCookies();
    static void cleanupSessions();

private:
    // 辅助方法
    string trim(const string& str);
    string generateSessionId();
    static string formatTime(time_t t);
};

#endif // COOKIE_H