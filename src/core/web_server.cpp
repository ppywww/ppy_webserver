#include "core/web_server.hpp"
#include "core/connection.hpp"
#include "core/http_request.hpp"
#include "core/http_response.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace ppsever {

// Pimpl实现类
class WebServer::Impl {
public:
    Impl(const Config& config, ThreadPool& thread_pool, EventLoop& event_loop)
        : config_(config), thread_pool_(thread_pool), event_loop_(event_loop) {}
    
    ~Impl() {
        Stop();
    }

    bool Start() {
        // 创建监听socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            return false;
        }

        // 设置socket选项
        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);

        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listen_fd_);
            return false;
        }

        // 开始监听
        if (listen(listen_fd_, config_.backlog) < 0) {
            close(listen_fd_);
            return false;
        }

        // 注册到事件循环
        event_loop_.AddFd(listen_fd_, EPOLLIN, [this](int fd) {
            HandleNewConnection(fd);
        });

        running_ = true;
        return true;
    }

    void Stop() {
        if (!running_) return;
        
        running_ = false;
        close(listen_fd_);
        
        // 关闭所有活跃连接
        for (auto& conn : connections_) {
            conn->Close();
        }
        connections_.clear();
    }

    void HandleNewConnection(int listen_fd) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) return;

        // 创建新连接
        auto connection = std::make_unique<Connection>(client_fd, thread_pool_);
        connections_.push_back(std::move(connection));
    }

private:
    Config config_;
    ThreadPool& thread_pool_;
    EventLoop& event_loop_;
    int listen_fd_ = -1;
    bool running_ = false;
    std::vector<std::unique_ptr<Connection>> connections_;
};

// WebServer 公共接口实现
WebServer::WebServer(const Config& config, ThreadPool& thread_pool, EventLoop& event_loop)
    : pimpl_(std::make_unique<Impl>(config, thread_pool, event_loop)) {}

WebServer::~WebServer() = default;

bool WebServer::Start() { return pimpl_->Start(); }
void WebServer::Stop() { pimpl_->Stop(); }
void WebServer::ForceStop() { pimpl_->Stop(); }

// 路由注册方法（简化实现）
void WebServer::Get(const std::string& path, RequestHandler handler) {
    // 路由表实现
}

void WebServer::Post(const std::string& path, RequestHandler handler) {}
void WebServer::Put(const std::string& path, RequestHandler handler) {}
void WebServer::Delete(const std::string& path, RequestHandler handler) {}
void WebServer::Any(const std::string& path, RequestHandler handler) {}
void WebServer::Static(const std::string& url_path, const std::string& file_path) {}

// 中间件方法
void WebServer::Use(Middleware middleware) {}
void WebServer::Use(const std::string& path, Middleware middleware) {}

// 状态查询
bool WebServer::IsRunning() const { return pimpl_->running_; }
size_t WebServer::GetActiveConnections() const { return pimpl_->connections_.size(); }
WebServer::Statistics WebServer::GetStatistics() const { return Statistics{}; }

// 事件回调
void WebServer::SetOnConnection(std::function<void(Connection&)> callback) {}
void WebServer::SetOnDisconnection(std::function<void(Connection&)> callback) {}
void WebServer::SetOnError(std::function<void(const std::string&)> callback) {}

} // namespace ppsever