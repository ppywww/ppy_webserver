#include "core/connection.hpp"
#include "core/web_server.hpp"
#include "core/event_loop.hpp"
#include "core/http_parser.hpp"
#include <system_error>
#include <cstring>
#include <algorithm>

namespace ppsever {

// 构造函数
Connection::Connection(int socket_fd, WebServer& server)
    : socket_fd_(socket_fd),
      server_(server),
      event_loop_(server.GetEventLoop()),
      state_(State::DISCONNECTED),
      create_time_(time(nullptr)),
      last_activity_time_(create_time_),
      max_buffer_size_(1048576),   // 默认1MB缓冲区
      timeout_seconds_(30) {       // 默认30秒超时
    
    // 验证文件描述符有效性
    if (socket_fd_ < 0) {
        throw std::invalid_argument("Invalid socket file descriptor");
    }
    // 获取远端地址信息
    socklen_t addr_len = sizeof(remote_addr_);
    if (getpeername(socket_fd_, reinterpret_cast<sockaddr*>(&remote_addr_), &addr_len) < 0) {
        throw std::system_error(errno, std::system_category(), "Failed to get peer address");
    }
    // 设置套接字选项
    SetupSocketOptions();
    // 初始状态设置为连接中
    state_ = State::CONNECTING;
    
    // ========================================================================
    std::cout << "Connection created, FD: " << socket_fd_ 
              << ", Remote: " << GetRemoteAddress() << std::endl;
    // ========================================================================
}

Connection::~Connection() {
    try {
        Close(); // 确保连接正确关闭
    } catch (const std::exception& e) {
        std::cerr << "Error during connection destruction: " << e.what() << std::endl;
    }
}

void Connection::Start() {
    if (state_ != State::CONNECTING) {
        return;
    }
    
    try {
        // 注册到事件循环监控读事件（边缘触发模式）
        event_loop_.AddFd(socket_fd_, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET,
            [self = shared_from_this()](int fd, uint32_t events) {
                self->HandleReadable();
            });
        
        // 更新状态为已连接
        state_ = State::CONNECTED;
        UpdateActivityTime();
        
        // 设置超时定时器
        event_loop_.RunAfter(timeout_seconds_ * 1000, [self = shared_from_this()]() {
            if (self->GetState() == State::CONNECTED && 
                time(nullptr) - self->GetLastActivityTime() > self->timeout_seconds_) {
                self->Close(); // 超时关闭连接
            }
        });
        
        std::cout << "Connection started, FD: " << socket_fd_ << std::endl;
    } catch (const std::exception& e) {
        NotifyError("Failed to start connection: " + std::string(e.what()));
        Close();
    }
}

// 读取数据
ssize_t Connection::ReadData() {
    if (state_ != State::CONNECTED && state_ != State::READING) {
        return -1;
    }
    
    char buffer[4096];
    ssize_t n = read(socket_fd_, buffer, sizeof(buffer));
    
    if (n > 0) {
        // 更新活动时间
        UpdateActivityTime();
        
        // 保护缓冲区访问
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        // 将数据追加到读缓冲区
        read_buffer_.append(buffer, n);
        
        // 检查缓冲区大小限制
        if (read_buffer_.size() > max_buffer_size_) {
            NotifyError("Read buffer overflow");
            Close();
            return -1;
        }
        
        // 更新状态
        state_ = State::READING;
        
        return n;
    } else if (n == 0) {
        // 对端关闭连接
        Close();
        return 0;
    } else {
        // 读取错误处理
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            NotifyError("Read error: " + std::string(strerror(errno)));
            Close();
        }
        return -1;
    }
}

// 写入数据
ssize_t Connection::WriteData(const std::string& data) {
    if (state_ != State::CONNECTED && state_ != State::WRITING) {
        return -1;
    }
    
    // 保护缓冲区访问
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // 追加数据到写缓冲区
    size_t old_size = write_buffer_.size();
    write_buffer_.append(data);
    
    // 检查缓冲区大小限制
    if (write_buffer_.size() > max_buffer_size_) {
        NotifyError("Write buffer overflow");
        Close();
        return -1;
    }
    
    // 如果写缓冲区非空，注册写事件监控
    if (write_buffer_.empty()) {
        event_loop_.UpdateFd(socket_fd_, 
            EventLoop::EPOLL_READ | EventLoop::EPOLL_WRITE | EventLoop::EPOLL_ET);
        
        state_ = State::WRITING;
    }
    
    return write_buffer_.size() - old_size;
}

// 尝试解析HTTP请求
bool Connection::TryParseHttpRequest() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (read_buffer_.empty()) {
        return false;
    }
    
    // 使用HTTP解析器解析请求
    auto& http_parser = server_.GetHttpParser();// 获取HTTP解析器
    auto request = http_parser.Parse(read_buffer_);
    
    if (request) {
        // 成功解析完整请求
        current_request_ = std::move(request);
        read_buffer_.clear(); // 清空已处理数据
        
        // 触发读取回调
        if (read_callback_) {
            read_callback_();
        }
        
        return true;
    }
    
    return false; // 需要更多数据
}

// 关闭连接
void Connection::Close() {
    if (state_ == State::DISCONNECTED || state_ == State::CLOSING) {
        return;
    }
    
    state_ = State::CLOSING;
    
    try {
        // 从事件循环中移除监控
        event_loop_.RemoveFd(socket_fd_);
        
        // 关闭套接字
        if (socket_fd_ >= 0) {
            shutdown(socket_fd_, SHUT_RDWR);
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        // 清理缓冲区和资源
        CleanupResources();
        
        // 触发关闭回调
        if (close_callback_) {
            close_callback_();
        }
        
        // 更新状态
        state_ = State::DISCONNECTED;
        
        std::cout << "Connection closed, FD: " << socket_fd_ << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during connection close: " << e.what() << std::endl;
    }
}

// 优雅关闭
void Connection::Shutdown() {
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_WR); // 只关闭写方向，允许读取剩余数据
    }
}

// 处理可读事件
void Connection::HandleReadable() {
    try {
        // 边缘触发模式，需要循环读取所有可用数据
        while (true) {
            ssize_t n = ReadData();
            if (n <= 0) {
                break;
            }
            
            // 尝试解析HTTP请求
            if (TryParseHttpRequest()) {
                break; // 成功解析一个完整请求，退出循环
            }
        }
    } catch (const std::exception& e) {
        NotifyError("Handle readable error: " + std::string(e.what()));
        Close();
    }
}

// 处理可写事件
void Connection::HandleWritable() {
    try {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        if (!write_buffer_.empty()) {
            // 发送写缓冲区中的数据
            ssize_t n = write(socket_fd_, write_buffer_.data(), write_buffer_.size());
            if (n > 0) {
                write_buffer_.erase(0, n); // 移除已发送数据
                UpdateActivityTime();
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                NotifyError("Write error: " + std::string(strerror(errno)));
                Close();
                return;
            }
        }
        
        // 如果写缓冲区为空，取消写事件监控
        if (write_buffer_.empty()) {
            event_loop_.UpdateFd(socket_fd_, 
                EventLoop::EPOLL_READ | EventLoop::EPOLL_ET);
            state_ = State::CONNECTED;
        }
    } catch (const std::exception& e) {
        NotifyError("Handle writable error: " + std::string(e.what()));
        Close();
    }
}

// 设置套接字选项
void Connection::SetupSocketOptions() {
    // 设置非阻塞模式
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::system_error(errno, std::system_category(), 
                               "Failed to set non-blocking mode");
    }
    
    // 设置TCP_NODELAY减少延迟
    int opt = 1;
    if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        throw std::system_error(errno, std::system_category(), 
                               "Failed to set TCP_NODELAY");
    }
    
    // 设置SO_KEEPALIVE启用心跳检测
    opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        throw std::system_error(errno, std::system_category(), 
                               "Failed to set SO_KEEPALIVE");
    }
}

// 获取远端地址字符串
std::string Connection::GetRemoteAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &remote_addr_.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(remote_addr_.sin_port));
}

// 清理资源
void Connection::CleanupResources() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    read_buffer_.clear();
    read_buffer_.shrink_to_fit();
    write_buffer_.clear();
    write_buffer_.shrink_to_fit();
    current_request_.reset();
    current_response_.reset();
}

// 错误通知
void Connection::NotifyError(const std::string& error_msg) {
    std::cerr << "Connection error, FD: " << socket_fd_ 
              << ", Error: " << error_msg << std::endl;
    
    if (error_callback_) {
        error_callback_(error_msg);
    }
}

// 其余getter和setter方法实现
Connection::State Connection::GetState() const { return state_; }
int Connection::GetFd() const { return socket_fd_; }
time_t Connection::GetLastActivityTime() const { return last_activity_time_; }
void Connection::UpdateActivityTime() { last_activity_time_ = time(nullptr); }
size_t Connection::GetReadBufferSize() const { std::lock_guard<std::mutex> lock(buffer_mutex_); return read_buffer_.size(); }
size_t Connection::GetWriteBufferSize() const { std::lock_guard<std::mutex> lock(buffer_mutex_); return write_buffer_.size(); }
void Connection::SetReadCallback(std::function<void()> callback) { read_callback_ = std::move(callback); }
void Connection::SetWriteCallback(std::function<void()> callback) { write_callback_ = std::move(callback); }
void Connection::SetCloseCallback(std::function<void()> callback) { close_callback_ = std::move(callback); }
void Connection::SetErrorCallback(std::function<void(const std::string&)> callback) { error_callback_ = std::move(callback); }
void Connection::SetTimeout(int seconds) { timeout_seconds_ = seconds; }
void Connection::SetMaxBufferSize(size_t size) { max_buffer_size_ = size; }

} // namespace ppsever