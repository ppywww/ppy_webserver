#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include "web_server.hpp"
#include "event_loop.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

using namespace ppserver;

int main() {
    try {
        // 创建事件循环
        EventLoop event_loop;
        
        // 创建连接管理器
        ConnectionManager conn_manager; 
        
        // 创建线程池
        ThreadPool thread_pool({4, 16, 1000, std::chrono::seconds(60)});
        
        // 配置服务器
        WebServer::Config config;
        config.host = "127.0.0.1";
        config.port = 8222;
        config.max_connections = 1000;
        config.backlog = 1024;
        
        // 启动服务器
        std::cout << "Starting HTTP server on " << config.host << ":" << config.port << std::endl;
        std::cout << "访问地址:  http://127.0.0.1:" <<config.port<<std::endl;
        bool server_started = false;

         uint16_t original_port = config.port;
        bool port_found = false;
        if (conn_manager.IsPortAvailable(config.host, config.port)) {
            std::cout << "Port " << config.port << " is available" << std::endl;
            port_found = true;
        } else {
            std::cout << "Port " << config.port << " is not available, searching for alternatives..." << std::endl;
            
            // 尝试最多10个端口
            for (int attempts = 1; attempts < 10; ++attempts) {
                uint16_t new_port = original_port + attempts;
                if (conn_manager.IsPortAvailable(config.host, new_port)) {
                    std::cout << "Found available port: " << new_port << std::endl;
                    const_cast<WebServer::Config&>(config).port = new_port;
                    port_found = true;
                    break;
                } else {
                    std::cout << "Port " << new_port << " is also not available" << std::endl;
                }
            }
        }
        
        if (!port_found) {
            std::cerr << "❌ 无法找到可用端口启动服务器" << std::endl;
            return 1;
        }

         std::unique_ptr<WebServer> server = std::make_unique<WebServer>(
           config, event_loop, conn_manager, thread_pool);
        
        // 设置信号处理
        server->SetSignalHandlers();
        
        // 服务器启动成功后再输出最终的访问地址
        std::cout << "✅ HTTP server successfully started on " << config.host << ":" << config.port << std::endl;
        std::cout << "访问地址:  http://127.0.0.1:" <<config.port<<std::endl;
        std::cout << "curl 测试命令:  curl http://127.0.0.1:" <<config.port<<"/"<<std::endl;
        std::cout << "curl 测试命令:  curl http://127.0.0.1:" <<config.port<<"/index.html"<<std::endl;
        std::cout << "curl 测试命令:  telnet 127.0.0.1 " <<config.port<<std::endl;
          
        event_loop.Run();
    
    return 0;
}   catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
}