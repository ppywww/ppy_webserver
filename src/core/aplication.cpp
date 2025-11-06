#include "application.hpp"//
#include "event_loop.hpp"
#include "web_server.hpp"
#include "thread_pool.hpp"
#include <iostream>
#include <csignal>
#include <cstring>

namespace ppsever {

// 静态成员初始化
Application* Application::instance_ = nullptr;
std::mutex Application::mutex_;

// 信号处理函数
volatile std::sig_atomic_t Application::signal_status_ = 0;

void Application::SignalHandler(int signal) {
    signal_status_ = signal;
    if (instance_) {
        instance_->Shutdown();
    }
}

Application& Application::GetInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
        instance_ = new Application();
    }
    return *instance_;
}

bool Application::Initialize(int argc, char** argv) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        std::cerr << "Application already initialized" << std::endl;
        return false;
    }

    try {
        // 1. 解析命令行参数
        if (!ParseArguments(argc, argv)) {
            std::cerr << "Failed to parse command line arguments" << std::endl;
            return false;
        }

        // 2. 加载配置文件
        if (!LoadConfig()) {
            std::cerr << "Failed to load configuration" << std::endl;
            return false;
        }

        // 3. 初始化各组件（按依赖顺序）
        std::cout << "Initializing thread pool..." << std::endl;
        thread_pool_ = std::make_unique<ThreadPool>(config_.thread_pool_size);
        
        std::cout << "Initializing event loop..." << std::endl;
        event_loop_ = std::make_unique<EventLoop>();
        
        std::cout << "Initializing web server..." << std::endl;
        web_server_ = std::make_unique<WebServer>(config_.server_port, *thread_pool_);

        // 4. 设置信号处理
        SetupSignalHandlers();

        initialized_ = true;
        std::cout << "Application initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }
}

int Application::Run() {
    if (!initialized_) {
        std::cerr << "Application not initialized. Call Initialize() first." << std::endl;
        return -1;
    }

    std::cout << "Starting application..." << std::endl;
    
    try {
        // 启动各组件
        thread_pool_->Start();
        web_server_->Start();
        
        std::cout << "Application running. Press Ctrl+C to stop." << std::endl;
        
        // 主事件循环
        return event_loop_->Run();
        
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        Shutdown();
        return -1;
    }
}

void Application::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) return;
    
    std::cout << "Shutting down application..." << std::endl;
    
    // 优雅关闭顺序：停止接收新请求 → 停止事件循环 → 停止工作线程
    if (web_server_) web_server_->Stop();
    if (event_loop_) event_loop_->Stop();
    if (thread_pool_) thread_pool_->Shutdown();
    
    Cleanup();
    initialized_ = false;
    
    std::cout << "Application shutdown complete" << std::endl;
}

// 获取组件实例
WebServer* Application::GetWebServer() const {
    return initialized_ ? web_server_.get() : nullptr;
}

ThreadPool* Application::GetThreadPool() const {
    return initialized_ ? thread_pool_.get() : nullptr;
}

EventLoop* Application::GetEventLoop() const {
    return initialized_ ? event_loop_.get() : nullptr;
}

// 私有辅助方法
bool Application::ParseArguments(int argc, char** argv) {
    // 简单的命令行参数解析
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0) {
            PrintHelp();
            return false;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config_.server_port = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            config_.thread_pool_size = std::stoi(argv[++i]);
        }
    }
    return true;
}

bool Application::LoadConfig() {
    // 这里可以添加配置文件加载逻辑
    // 例如：从JSON/XML文件读取配置
    return true; // 暂时直接返回成功
}

void Application::SetupSignalHandlers() {
    std::signal(SIGINT, SignalHandler);   // Ctrl+C
    std::signal(SIGTERM, SignalHandler);  // kill命令
    std::signal(SIGPIPE, SIG_IGN);        // 忽略管道破裂信号
}

void Application::PrintHelp() {
    std::cout << "Usage: ppsever [options]\n"
              << "Options:\n"
              << "  --port <number>     Server port (default: 8080)\n"
              << "  --threads <number> Thread pool size (default: 4)\n"
              << "  --help             Show this help message\n";
}

void Application::Cleanup() {
    // 逆序清理资源
    web_server_.reset();
    event_loop_.reset();
    thread_pool_.reset();
}

// 析构函数
Application::~Application() {
    Shutdown();
}

} // namespace ppsever