#include "connection.hpp"
#include "web_server.hpp"
#include "event_loop.hpp"
#include "http_parser.hpp"
#include <system_error>
#include <cstring>
#include <algorithm>
#include <netinet/tcp.h>

namespace ppserver {

// 构造函数
Connection::Connection(int socket_fd, WebServer& server)
    : socket_fd_(socket_fd),
      state_(State::DISCONNECTED),
      server_(server),
      event_loop_(server.GetEventLoop()),
      create_time_(time(nullptr)),
      last_activity_time_(create_time_),
      max_buffer_size_(1048576),   // 默认1MB缓冲区
      timeout_seconds_(30) {       
    

    if (socket_fd_ < 0) {
        throw std::invalid_argument("Invalid socket file descriptor");
    }
    socklen_t addr_len = sizeof(remote_addr_);
    if (getpeername(socket_fd_, reinterpret_cast<sockaddr*>(&remote_addr_), &addr_len) < 0) {
        throw std::system_error(errno, std::system_category(), "Failed to get peer address");
    }
    SetupSocketOptions();
    state_ = State::CONNECTING;
    
}

Connection::~Connection() {
    try {
        Close(); // 确保连接正确关闭
    } catch (const std::exception& e) {
        std::cerr << "Error during connection destruction: " << e.what() << std::endl;
    }
}

void Connection::SetHandler(std::shared_ptr<Handler> handler) {
    handler_ = std::move(handler);
}
void Connection::Start() {
    // 注册到事件循环
    // event_loop_.AddFd(socket_fd_, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET,
    //     [self = shared_from_this()](int , uint32_t ) {
    //         self->HandleReadable();
    //     });
    
    state_ = State::CONNECTED;

    UpdateActivityTime();
    // Handler切入连接建立流程的入口点
    if (handler_) {
        handler_->OnConnection(shared_from_this());
    }
}

void Connection::Close() {
    if (state_ == State::DISCONNECTED || state_ == State::CLOSING) {
        return;
    }
    
    state_ = State::CLOSING;
    
    // 从事件循环中移除监控
    event_loop_.RemoveFd(socket_fd_);
    
    // 关闭套接字
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    // Handler切入连接关闭流程的入口点
    if (handler_) {
        handler_->OnDisconnection(shared_from_this());
    }
    
    // 清理资源
    CleanupResources();
    state_ = State::DISCONNECTED;
}

// 读取数据
ssize_t Connection::ReadData() {
    if (state_ != State::CONNECTED && state_ != State::READING) {
        return -1;
    }
    char buffer[4096];
    ssize_t n = read(socket_fd_, buffer, sizeof(buffer));// *****读取数据*****
    
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
ssize_t Connection::WriteData(const std::string& data) {//给handler自实现handlewrite用的
    // if (state_ != State::CONNECTED && state_ != State::WRITING) {
    //     return -1;
    // }
    std::cout << "write_buffer_size111:" << write_buffer_.size() << std::endl;
    // 保护缓冲区访问
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // 追加数据到写缓冲区
    size_t old_size = write_buffer_.size();
    write_buffer_.append(data);
    std::cout << "write_buffer_size222:" << write_buffer_.size() << std::endl;
    
    // 检查缓冲区大小限制
    if (write_buffer_.size() > max_buffer_size_) {
        NotifyError("Write buffer overflow");
        Close();
        return -1;
    }
    
    // 如果写缓冲区非空，注册写事件监控
    if ( ! write_buffer_.empty()) {
        ////////////////////////////////////////////updatefd只关注写事件////////////////////////////////////
        event_loop_.UpdateFd(socket_fd_, 
            EventLoop::EPOLL_READ | EventLoop::EPOLL_WRITE | EventLoop::EPOLL_ET);//*****注册写事件*****
        
        state_ = State::WRITING;
    }
    
    return write_buffer_.size() - old_size;
}


void Connection::HandleReadable() {


    // 这是Handler切入事件处理流程的入口点
    if (handler_) {

        handler_->HandleRead(shared_from_this());
    } else {

        DefaultHandleRead();
    }
}

void Connection::HandleWritable() {
    if (handler_) {
        handler_->HandleWrite(shared_from_this());
    } else {
        DefaultHandleWrite();
    }
}

void Connection::HandleError() {
    if (handler_) {
        handler_->HandleError(shared_from_this());
    } else {
        DefaultHandleError();
    }
}


std::string Connection::GetReadBuffer() const{
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return read_buffer_;
}
    void Connection::ClearReadBuffer(){
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        read_buffer_.clear();
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

std::string Connection::GetRemoteAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &remote_addr_.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(remote_addr_.sin_port));
}


void Connection::CleanupResources() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    read_buffer_.clear();
    read_buffer_.shrink_to_fit();
    write_buffer_.clear();
    write_buffer_.shrink_to_fit();
}


void Connection::NotifyError(const std::string& error_msg) {
    std::cerr << "Connection error, FD: " << socket_fd_ 
              << ", Error: " << error_msg << std::endl;
    
    if (error_callback_) {
        error_callback_(error_msg);
    }
}

void Connection::DefaultHandleRead() {
    // 默认读取处理逻辑

    ssize_t bytes_read = ReadData();

    if (bytes_read > 0) {
        // 触发读回调
        if (read_callback_) {
            read_callback_();
        }
    }
}

 

void Connection::DefaultHandleWrite() {//buffer写到socket
    // 默认写处理逻辑
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (!write_buffer_.empty()) {//如果缓冲区非空
        ssize_t n = write(socket_fd_, write_buffer_.c_str(), write_buffer_.size());
        if (n > 0) {//如果写入成功
            // 移除已写入的数据
            write_buffer_.erase(0, n);
            
            // 如果缓冲区已清空
            if (write_buffer_.empty()) {
                // //////////////////////更新事件监控，只关注读事件/////////////////////
                event_loop_.UpdateFd(socket_fd_, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET);
                state_ = State::CONNECTED;
                
                // 触发写回调 
                if (write_callback_) {
                    write_callback_();
                }
            }
        } else if (n < 0) {
            // 处理写错误
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                NotifyError("Write error: " + std::string(strerror(errno)));
                Close();
            }
        }
    }
}
void Connection::DefaultHandleError() {
    // 默认错误处理逻辑
    NotifyError("Epoll event error");
    Close();
}

bool Connection::TryParseHttpRequest() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (read_buffer_.empty()) {
        return false;
    }
    
    auto request = http_parser_.Parse(read_buffer_.c_str(),read_buffer_.length());
    
    if (request.success) {
    
        read_buffer_.clear(); // 清空已处理数据
        
        if (read_callback_) {
            read_callback_();
        }
        
        return true;
    }
    
    return false; 
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