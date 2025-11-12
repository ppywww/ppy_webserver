#pragma once

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

#include "event_loop.hpp"
#include "thread_pool.hpp"
// #include "connection_manager.hpp"
#include "connection.hpp"
#include "http_parser.hpp"

/*
WebServer 类定义了一个基于事件驱动的高性能 HTTP 服务器框架，支持路由注册、中间件、连接管理等功能。

*/


namespace ppsever {

// 前置声明
class HttpRequest;
class HttpResponse;
class Connection;
class HttpParser;
class ThreadPool;

// HTTP 请求处理函数类型
using RequestHandler = std::function<std::unique_ptr<HttpResponse>(std::unique_ptr<HttpRequest>)>;

// 中间件函数类型（可用于日志、认证等）
using Middleware = std::function<bool(std::unique_ptr<HttpRequest>&, std::unique_ptr<HttpResponse>&)>;

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

    

    WebServer(const Config& config, 
              ThreadPool& thread_pool, 
              EventLoop& event_loop);
    ~WebServer();

    // 禁止拷贝和移动
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    WebServer(WebServer&&) = delete;
    WebServer& operator=(WebServer&&) = delete;

  
    bool Start();

    void Stop();

    // ========== 路由注册 API ==========
    void Get(const std::string& path, RequestHandler handler);
    void Post(const std::string& path, RequestHandler handler);
    void Put(const std::string& path, RequestHandler handler);
    void Delete(const std::string& path, RequestHandler handler);
    void Any(const std::string& path, RequestHandler handler);

    void Static(const std::string& url_path, const std::string& file_path);

    // ========== 中间件 API ==========

    void Use(Middleware middleware);

    void Use(const std::string& path, Middleware middleware);

    // ========== 状态查询 API ==========
    bool IsRunning() const;
    size_t GetActiveConnections() const;
    struct Statistics {
        size_t total_requests;
        size_t active_connections;
        size_t bytes_sent;
        size_t bytes_received;
    };
    Statistics GetStatistics() const;


    // ========== 设置事件回调 API ==========
    void SetOnConnection(std::function<void(Connection&)> callback);
    void SetOnDisconnection(std::function<void(Connection&)> callback);
    void SetOnError(std::function<void(const std::string&)> callback);
     
    EventLoop& GetEventLoop() {
        return event_loop_;
    }
    //http解析器
   
    HttpParser& GetHttpParser(); 

private:
    // 添加缺失的成员变量
    Config config_;
    ThreadPool& thread_pool_;
    EventLoop& event_loop_;
    HttpParser http_parser_;
    
    // 其他需要的成员变量
    bool running_ = false;
    int listen_fd_ = -1;
    
    // 连接管理
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    mutable std::mutex connections_mutex_;
    
    // 路由管理
    std::unordered_map<std::string, RequestHandler> routes_;
    mutable std::mutex routes_mutex_;
    
    // 中间件管理
    std::vector<Middleware> global_middlewares_;
    std::vector<std::pair<std::string, Middleware>> route_middlewares_;
    mutable std::mutex middleware_mutex_;
    
    // 回调函数
    std::function<void(Connection&)> on_connection_callback_;
    std::function<void(Connection&)> on_disconnection_callback_;
    std::function<void(const std::string&)> on_error_callback_;
    
    // 处理新连接
    void HandleNewConnection(int listen_fd);

  
};

} // namespace ppsever