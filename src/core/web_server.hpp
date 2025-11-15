#pragma once

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>
#include "event_loop.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"
#include "http_parser.hpp"

/*
WebServer 类定义了一个基于事件驱动的高性能 HTTP 服务器框架，支持路由注册、中间件、连接管理等功能。
WebServer 应该专注于
控制+开始监听
*/


namespace ppserver {

// 前置声明
class HttpRequest;
class HttpResponse;
class Connection;
class HttpParser;
class ThreadPool;
class ConnectionManager;


class WebServer {
public:

    struct Config {
        std::string host = "192.168.125.128";      // 监听地址
        uint16_t port = 8888;              // 监听端口
        size_t max_connections = 10000;     // 最大连接数
        int backlog = 1024;                 // 连接队列长度
        size_t max_request_size = 1024 * 1024; // 最大请求大小
        int timeout_seconds = 30;           // 连接超时时间
    };
   
    void Stop();
    bool Start();

    bool IsRunning();

    WebServer(
            Config& config,
            EventLoop& event_loop,
            ConnectionManager& connection_manager,
            ThreadPool& thread_pool
            );
    ~WebServer();

    void HandleNewConnection(int listen_fd, WebServer& server);


    EventLoop& GetEventLoop() const;

   

    // 禁止拷贝和移动
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    WebServer(WebServer&&) = delete;
    WebServer& operator=(WebServer&&) = delete;


     // 信号处理相关方法
    void SetSignalHandlers();
    static void SignalHandler(int signal);
    void HandleSignal(int signal);

          
    
private:

    // 添加缺失的成员变量
    Config &config_;
    EventLoop& event_loop_;
    ConnectionManager& connection_manager_;
    ThreadPool& thread_pool_;
    

    bool running_ = false;
    int listen_fd_ = -1;

    // 用于信号处理的静态成员
    static WebServer* instance_;
    

    std::function<void(Connection&)> on_connection_callback_;
    std::function<void(Connection&)> on_disconnection_callback_;
    std::function<void(const std::string&)> on_error_callback_;
    

};

} // namespace ppsever