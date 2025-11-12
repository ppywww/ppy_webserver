#include "web_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <memory>

#include <cstring>
#include <cerrno>
namespace ppsever {

WebServer::WebServer(const Config& config, ThreadPool& thread_pool, EventLoop& event_loop)
    : config_(config), thread_pool_(thread_pool), event_loop_(event_loop) {}

WebServer::~WebServer() {
    Stop();
}

bool WebServer::Start() {
    if (running_) {
        return true;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_fd_);
        return false;
    }

    if (listen(listen_fd_, config_.backlog) < 0) {
        close(listen_fd_);
        return false;
    }
    //注册监听listen_fd_的可读事件回调
    event_loop_.AddFd(listen_fd_, EventLoop::EPOLL_READ, [this](int fd, uint32_t events) {
        HandleNewConnection(fd);
    });

    running_ = true;
    return true;
}

void WebServer::Stop() {
    if (!running_) return;
    
    running_ = false;
    //关闭监听事件
    event_loop_.RemoveFd(listen_fd_);
    close(listen_fd_);
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [fd, conn] : connections_) {
        conn->Close();
    }
    connections_.clear();
}
//处理新连接的callback函数
void WebServer::HandleNewConnection(int listen_fd) {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept4(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), 
                           &addr_len, SOCK_NONBLOCK);
    if (client_fd < 0) {
        if (on_error_callback_) {
            on_error_callback_("Accept failed: " + std::string(strerror(errno)));
        }
        return;
    }

    auto conn = std::make_unique<Connection>(client_fd, *this);

    //注册客户端连接的可读事件回调
    event_loop_.AddFd(client_fd, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET, 
        [conn = conn.get()](int fd, uint32_t events) {
            conn->HandleReadable();
        });
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[client_fd] = std::move(conn);
    }
    
    if (on_connection_callback_) {
        on_connection_callback_(*connections_[client_fd]);
    }
}

HttpParser& WebServer::GetHttpParser(){
        return http_parser_;
    }

void WebServer::Get(const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_["GET:" + path] = std::move(handler);
}

void WebServer::Post(const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_["POST:" + path] = std::move(handler);
}

void WebServer::Put(const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_["PUT:" + path] = std::move(handler);
}

void WebServer::Delete(const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_["DELETE:" + path] = std::move(handler);
}

void WebServer::Any(const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_["ANY:" + path] = std::move(handler);
}

void WebServer::Static(const std::string& url_path, const std::string& file_path) {
    // 静态文件服务实现
}

void WebServer::Use(Middleware middleware) {
    std::lock_guard<std::mutex> lock(middleware_mutex_);
    global_middlewares_.push_back(std::move(middleware));
}

void WebServer::Use(const std::string& path, Middleware middleware) {
    std::lock_guard<std::mutex> lock(middleware_mutex_);
    route_middlewares_.emplace_back(path, std::move(middleware));
}

// ========== 状态查询 API ==========
bool WebServer::IsRunning() const {
    return running_;
}

size_t WebServer::GetActiveConnections() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

WebServer::Statistics WebServer::GetStatistics() const {
    Statistics stats;
    stats.active_connections = GetActiveConnections();
    return stats;
}
void WebServer::SetOnConnection(std::function<void(Connection&)> callback) {
    on_connection_callback_ = std::move(callback);
}

void WebServer::SetOnDisconnection(std::function<void(Connection&)> callback) {
    on_disconnection_callback_ = std::move(callback);
}

void WebServer::SetOnError(std::function<void(const std::string&)> callback) {
    on_error_callback_ = std::move(callback);
}

} // namespace ppsever