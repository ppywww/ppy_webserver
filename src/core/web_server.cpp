#include "web_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <memory>

#include <cstring>
#include <cerrno>
namespace ppserver {
    WebServer* WebServer::instance_ = nullptr;

WebServer::WebServer(Config& config, 
                     EventLoop& event_loop,
                     ConnectionManager& connection_manager
                     ,ThreadPool& thread_pool
                     )
    : config_(config), 
    event_loop_(event_loop),
    connection_manager_(connection_manager)
    ,thread_pool_(thread_pool) {
        instance_ = this;
}

WebServer::~WebServer() {
    Stop();
}


bool WebServer::Start() {
    if (running_) {
        return true;
    }

    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << config_.host << std::endl;
        close(listen_fd_);
        return false;
    }

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to " << config_.host << ":" << config_.port 
                  << ": " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }

    if (listen(listen_fd_, config_.backlog) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        close(listen_fd_);
        return false;
    }
    
    // 注册监听listen_fd_的可读事件回调
    event_loop_.AddFd(listen_fd_, EventLoop::EPOLL_READ, [this](int fd, uint32_t /*events*/) {
        HandleNewConnection(fd, *this);
    });

    running_ = true;
    return true;
}
void WebServer::Stop() {
    if (!running_) return;

    std::cout << "Stopping server..." << std::endl;
    
    running_ = false;
    // 关闭监听事件
    if (listen_fd_ >= 0) {
        event_loop_.RemoveFd(listen_fd_);
        close(listen_fd_);
        listen_fd_ = -1;
    }
    connection_manager_.CloseAllConnections();
    event_loop_.Stop();

    std::cout << "Web server stopped" << std::endl;

}

bool WebServer::IsRunning() {
    return running_;
}

EventLoop& WebServer::GetEventLoop() const {
    return event_loop_;
}

void WebServer::SetSignalHandlers() {
    signal(SIGINT, SignalHandler);   // Ctrl+C
    signal(SIGTERM, SignalHandler);  // 终止信号
}

void WebServer::SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    
    if (instance_) {
        instance_->HandleSignal(signal);
    }
}

void WebServer::HandleSignal(int signal) {
    std::cout << "Handling signal " << signal << std::endl;
    Stop();
    event_loop_.Stop();
}


void WebServer::HandleNewConnection(int listen_fd, WebServer& server) {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    std::cout << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
    
    int client_fd = accept4(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), 
                           &addr_len, SOCK_NONBLOCK);
    if (client_fd < 0) {
        if (on_error_callback_) {
            on_error_callback_("Accept failed: " + std::string(strerror(errno)));
        }
        return;
    }

    // 创建连接对象
    auto conn = std::make_shared<Connection>(client_fd, server);
  
    //===================设置自定义有的处理器=============================================================
    auto handler = std::make_shared<Handler>(event_loop_, *conn, thread_pool_);
    conn->SetHandler(handler);


    // 注册客户端连接的可读事件回调
    
    event_loop_.AddFd(client_fd, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET, 
        [conn](int , uint32_t events) {
            if(events & EventLoop::EPOLL_READ) {
                conn->HandleReadable();
            }
            if(events & EventLoop::EPOLL_WRITE) {
                conn->HandleWritable();
            }
            if(events & EventLoop::EPOLL_ERROR) {
                conn->HandleError();
            }
        });

    // 将连接添加到管理器中
    connection_manager_.AddConnection(client_fd, conn);
    
    // 启动连接
    conn->Start();
    // 触发连接回调
    if (on_connection_callback_) {
        on_connection_callback_(*conn);
    }
}

} // namespace ppsever