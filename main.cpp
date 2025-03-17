#include "./config/config.h"
#include <thread>

void start_cleanup_thread(){
    while(true){
        sleep(3600);
        Cookie::cleanupSessions();
    }
}

int main(int argc, char *argv[])
{
    //需要修改的数据库信息,登录名,密码,库名
    string user = "root";
    string passwd = "bd20030423";
    string databasename = "yourdb";

    //命令行解析（根据用户的输入对基本配置进行修改）
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    thread cleanup_thread(start_cleanup_thread);
    cleanup_thread.detach();

    //运行
    server.eventLoop();

    return 0;
}