#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>
#include <system_error>
#include <sstream>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "event_loop.hpp"
#include "connection.hpp"
#include "connection_manager.hpp"

using namespace ppsever;

/**
 * 回发连接处理器 - 处理客户端连接和数据回发
 */
class EchoConnectionHandler {
public:
    EchoConnectionHandler(int client_fd, EventLoop& loop) 
        : client_fd_(client_fd), event_loop_(loop) {
        // 设置非阻塞模式
        int flags = fcntl(client_fd_, F_GETFL, 0);
        fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK);
        
        // 获取客户端地址信息
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        getpeername(client_fd_, (sockaddr*)&client_addr, &addr_len);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        client_address_ = std::string(ip) + ":" + std::to_string(ntohs(client_addr.sin_port));
    }
    
    /**
     * 启动连接监控
     */
    /**
 * 启动连接监控
 */
void Start() {
    // 注册到事件循环，监控读事件（边缘触发模式）
  
    
    // 使用RunInLoop确保在事件循环线程中执行
    event_loop_.RunInLoop([this]() {
        event_loop_.AddFd(client_fd_, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET,
            [this](int fd, uint32_t events) {
                HandleReadable();
            });
        
        std::cout << "📞 客户端连接: " << client_address_ << std::endl;
        
        // 发送欢迎消息
        SendWelcomeMessage();
    });
}
    
    /**
     * 处理可读事件 - 实现回发功能
     */
    void HandleReadable() {
        char buffer[1024];
        ssize_t total_read = 0;
        
        // 边缘触发模式，循环读取所有可用数据
        while (true) {
            std::cout << "开始 Received " << std::endl;
            ssize_t n = read(client_fd_, buffer + total_read, sizeof(buffer) - total_read - 1);
            
            if (n > 0) {
                total_read += n;
                
                // 检查缓冲区是否接近满
                if (total_read >= sizeof(buffer) - 1) {
                    ProcessReceivedData(buffer, total_read);
                    total_read = 0;
                }
            } else if (n == 0) {
                // 连接关闭
                std::cout << "🔌 连接关闭: " << client_address_ << std::endl;
                Close();
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // 无更多数据可读
                } else {
                    std::cerr << "❌ 读取错误 [" << client_address_ << "]: " 
                              << strerror(errno) << std::endl;
                    Close();
                    return;
                }
            }
        }
        
        if (total_read > 0) {
            ProcessReceivedData(buffer, total_read);
        }
    }
    
    /**
     * 处理接收到的数据并回发
     */
    void ProcessReceivedData(const char* data, size_t length) {
        // 确保数据以null结尾
        std::string received_data(data, length);
        
        // 按行处理（适用于telnet）
        std::istringstream stream(received_data);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                // 移除可能的回车符
                if (line.back() == '\r') {
                    line.pop_back();
                }
                
                if (!line.empty()) {
                    std::cout << "📥 收到数据 [" << client_address_ << "]: " << line << std::endl;
                    
                    // 回发相同数据
                    std::string response = line + "\r\n"; // Telnet期望CRLF换行
                    SendResponse(response);
                }
            }
        }
    }
    
    /**
     * 发送欢迎消息
     */
    void SendWelcomeMessage() {
        std::string welcome = "欢迎使用回发服务器! 输入任何文本将回发相同内容.\r\n";
        welcome += "输入 'quit' 或 Ctrl+] 然后 quit 退出连接.\r\n";
        SendResponse(welcome);
    }
    
    /**
     * 发送响应数据
     */
    void SendResponse(const std::string& response) {
        ssize_t n = write(client_fd_, response.c_str(), response.length());
        if (n < 0) {
            std::cerr << "❌ 发送失败 [" << client_address_ << "]: " 
                      << strerror(errno) << std::endl;
            Close();
        } else if (n > 0) {
            std::cout << "📤 发送响应 [" << client_address_ << "]: " 
                      << response.substr(0, response.length() - 2); // 移除CRLF用于日志
        }
    }
    

    void Close() {
        if (client_fd_ >= 0) {
            event_loop_.RemoveFd(client_fd_);
            ::close(client_fd_);
            client_fd_ = -1;
            std::cout << "🗑️  连接清理: " << client_address_ << std::endl;
        }
    }
    
    ~EchoConnectionHandler() {
        Close();
    }

private:
    int client_fd_;
    EventLoop& event_loop_;
    std::string client_address_;
};

/**
 * 回发服务器类
 */
class EchoServer {
public:
    EchoServer(uint16_t port) : port_(port), running_(false), listen_fd_(-1) {}
    
    /**
     * 启动回发服务器 - 关键修改：只监听192.168.125.128
     */
    bool Start() {
        // 创建监听socket
        listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listen_fd_ < 0) {
            std::cerr << "❌ 创建socket失败: " << strerror(errno) << std::endl;
            return false;
        }
        
        // 设置端口复用
        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // 绑定地址 - 修改为只监听特定IP 192.168.125.128
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        
        // 关键修改：使用特定IP而不是INADDR_ANY
        if (inet_pton(AF_INET, "192.168.125.128", &server_addr.sin_addr) <= 0) {
            std::cerr << "❌ 无效的IP地址: 192.168.125.128" << std::endl;
            close(listen_fd_);
            return false;
        }
        server_addr.sin_port = htons(port_);
        
        if (bind(listen_fd_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "❌ 绑定端口失败: " << strerror(errno) 
                      << " (IP: 192.168.125.128, Port: " << port_ << ")" << std::endl;
            close(listen_fd_);
            return false;
        }
        
        // 开始监听
        if (listen(listen_fd_, 128) < 0) {
            std::cerr << "❌ 监听失败: " << strerror(errno) << std::endl;
            close(listen_fd_);
            return false;
        }
        
        // 注册监听socket到事件循环
        event_loop_.AddFd(listen_fd_, EventLoop::EPOLL_READ | EventLoop::EPOLL_ET,
            [this](int fd, uint32_t events) {
                HandleNewConnection();
            });
        
        running_ = true;
        std::cout << "🚀 回发服务器启动成功, 监听地址: 192.168.125.128:" << port_ << std::endl;
        std::cout << "💡 使用命令测试: telnet 192.168.125.128 " << port_ << std::endl;
        
        return true;
    }
    
    /**
     * 运行事件循环
     */
    void Run() {
        event_loop_.Run();
    }
    
    /**
     * 停止服务器
     */
    void Stop() {
        running_ = false;
        event_loop_.Stop();
        
        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
        }
        
        // 清理所有连接
        for (auto& handler : connection_handlers_) {
            handler->Close();
        }
        connection_handlers_.clear();
        
        std::cout << "🛑 回发服务器已停止" << std::endl;
    }

private:

    void HandleNewConnection() {
        
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        // 接受新连接
        int client_fd = accept4(listen_fd_, (sockaddr*)&client_addr, &addr_len, SOCK_NONBLOCK);

        if (client_fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "❌ 接受连接失败: " << strerror(errno) << std::endl;
            }
            return;
        }
        
        auto handler = std::make_shared<EchoConnectionHandler>(client_fd, event_loop_);
        handler->Start();
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connection_handlers_.push_back(handler);
    }
    
    uint16_t port_;
    int listen_fd_;
    bool running_;
    EventLoop event_loop_;
    
    std::vector<std::shared_ptr<EchoConnectionHandler>> connection_handlers_;
    std::mutex connections_mutex_;
};

void SetupSignalHandlers(EchoServer& server) {
    struct sigaction sa;
    sa.sa_handler = [](int sig) {
        std::cout << "\n🛑 收到信号 " << sig << ", 正在关闭服务器..." << std::endl;
        exit(0);
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, nullptr);  // Ctrl+C
    sigaction(SIGTERM, &sa, nullptr); // 终止信号
}
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "       回发服务器功能测试                 " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    uint16_t PORT = 8888;
    
    try {
        std::unique_ptr<EchoServer> server;
        bool server_started = false;
        
        // 尝试启动服务器，如果端口被占用则尝试下一个端口
        for (int attempts = 0; attempts < 10; ++attempts) {
            server = std::make_unique<EchoServer>(PORT);
            
            if (server->Start()) {
                server_started = true;
                break;
            } else {
                // 检查是否是地址已被使用错误
                if (errno == EADDRINUSE) {
                    std::cout << "⚠️  端口 " << PORT << " 已被占用，尝试端口 " << (PORT + 1) << std::endl;
                    PORT++;
                } else {
                    // 其他错误，直接退出
                    std::cerr << "❌ 服务器启动失败" << std::endl;
                    return 1;
                }
            }
        }
        
        if (!server_started) {
            std::cerr << "❌ 无法找到可用端口启动服务器" << std::endl;
            return 1;
        }
          
     
        
        std::cout << "\n🎯 回发服务器运行中..." << std::endl;
        std::cout << "💡 使用以下命令测试:" << std::endl;
        std::cout << "   telnet 192.168.125.128 " << PORT << std::endl;
        std::cout << "   或" << std::endl;
        std::cout << "   nc 192.168.125.128 " << PORT << std::endl;
        std::cout << "   Ctrl+C 退出服务器" << std::endl;
        std::cout << "==========================================" << std::endl;
      
           // 运行事件循环
        server->Run();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 服务器异常: " << e.what() << std::endl;
        return 1;
    }
}