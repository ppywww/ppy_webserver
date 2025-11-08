#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <atomic>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <iostream>
#include "core/http_request.hpp"
#include "core/http_response.hpp"

namespace ppsever {

// 前置声明
class WebServer;
class EventLoop;

/**
 * Connection - TCP连接管理类
 * 职责：封装单个TCP连接的生命周期、数据读写和协议处理
 * 特性：线程安全、资源自动管理、边缘触发优化、超时控制
 */
class Connection : public std::enable_shared_from_this<Connection> {//使得类的实例能够安全地生成指向自身的shared_ptr
public:
    // 连接状态枚举
    enum class State {
        DISCONNECTED,   // 未连接状态
        CONNECTING,     // 连接建立中
        CONNECTED,      // 已连接，活跃状态
        READING,        // 数据读取中
        WRITING,        // 数据写入中
        CLOSING         // 连接关闭中
    };

    // 构造函数与析构函数
    Connection(int socket_fd, WebServer& server);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    // 生命周期管理
    void Start();
    void Close();
    void Shutdown();

    // 数据读写操作
    ssize_t ReadData();
    ssize_t WriteData(const std::string& data);
    bool TryParseHttpRequest();

    // 状态查询与信息获取
    State GetState() const;
    int GetFd() const;
    std::string GetRemoteAddress() const;
    time_t GetLastActivityTime() const;
    size_t GetReadBufferSize() const;
    size_t GetWriteBufferSize() const;

    // 回调设置接口
    void SetReadCallback(std::function<void()> callback);
    void SetWriteCallback(std::function<void()> callback);
    void SetCloseCallback(std::function<void()> callback);
    void SetErrorCallback(std::function<void(const std::string&)> callback);

    // 事件处理接口
    void HandleReadable();
    void HandleWritable();
    void HandleError();

    // 配置接口
    void SetTimeout(int seconds);
    void SetMaxBufferSize(size_t size);

private:
    // 内部辅助方法
    void SetupSocketOptions();
    void UpdateActivityTime();
    void CleanupResources();
    void NotifyError(const std::string& error_msg);
    void SendResponse();

    // 成员变量
    int socket_fd_;                         // 套接字文件描述符
    State state_;                           // 当前连接状态
    WebServer& server_;                     // 所属服务器引用
    EventLoop& event_loop_;                 // 事件循环引用
    
    // 数据缓冲区
    std::string read_buffer_;               // 读数据缓冲区
    std::string write_buffer_;              // 写数据缓冲区
    
    // HTTP协议处理
    std::unique_ptr<HttpRequest> current_request_;
    std::unique_ptr<HttpResponse> current_response_;
    
    // 回调函数
    std::function<void()> read_callback_;
    std::function<void()> write_callback_;
    std::function<void()> close_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // 连接信息
    sockaddr_in remote_addr_;               // 远端地址信息
    time_t create_time_;                   // 连接创建时间
    time_t last_activity_time_;            // 最后活动时间
    
    // 配置参数
    size_t max_buffer_size_;               // 缓冲区最大大小
    int timeout_seconds_;                  // 超时时间（秒）
    
    // 线程安全
    mutable std::mutex buffer_mutex_;      // 缓冲区访问互斥锁
};

} // namespace ppsever