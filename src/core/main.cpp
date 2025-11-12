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




int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "           Web服务器功能测试              " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    uint16_t PORT = 8888;
 
    
    try {
        // 创建事件循环和线程池
        EventLoop event_loop;
        // 创建4个线程的线程池
        
        // 配置Web服务器
     
       ThreadPool::Config_thread_pool thread_config;
        thread_config.core_threads = 4;
        
        // 创建线程池
        ThreadPool thread_pool(thread_config);

        ppsever::WebServer::Config config;
        config.port = PORT;
        config.host = "192.168.125.128";
        
        // 创建Web服务器实例
        ppsever::WebServer server(config, thread_pool, event_loop);
        


        
        // 注册简单的路由处理函数
        server.Get("/", [](std::unique_ptr<HttpRequest> req) {
            auto response = std::make_unique<HttpResponse>();
            response->SetStatusCode(HttpResponse::HttpStatusCode::OK);
            response->SetHeader("Content-Type", "text/html; charset=utf-8");
            response->SetBody("<h1>欢迎使用PPServer!</h1><p>这是一个简单的Web服务器测试页面。</p>");
            return response;
        });
        
        server.Get("/echo", [&server](std::unique_ptr<HttpRequest> req) {
            auto response = std::make_unique<HttpResponse>();
            response->SetStatusCode(HttpResponse::HttpStatusCode::OK);
            response->SetHeader("Content-Type", "text/plain; charset=utf-8");
            
            std::string body = "Echo Server Running!\n";
            body += "当前活跃连接数: " + std::to_string(server.GetActiveConnections()) + "\n";
            response->SetBody(body);
            return response;
        });
        
        // 启动服务器
        bool server_started = false;
        for (int attempts = 0; attempts < 10; ++attempts) {
            config.port = PORT;
            if (server.Start()) {
                server_started = true;
                break;
            } else {
                // 检查是否是地址已被使用错误
                if (errno == EADDRINUSE) {
                    std::cout << "⚠️  端口 " << PORT << " 已被占用，尝试端口 " << (PORT + 1) << std::endl;
                    PORT++;
                } else {
                    // 其他错误，直接退出
                    std::cerr << "❌ 服务器启动失败: " << strerror(errno) << std::endl;
                    return 1;
                }
            }
        }
        
        if (!server_started) {
            std::cerr << "❌ 无法找到可用端口启动服务器" << std::endl;
            return 1;
        }
        
        std::cout << "\n🎯 Web服务器运行中..." << std::endl;
        std::cout << "💡 使用以下方式访问:" << std::endl;
        std::cout << "   curl http://192.168.125.128:" << PORT << "/" << std::endl;
        std::cout << "   curl http://192.168.125.128:" << PORT << "/echo" << std::endl;
        std::cout << "   或在浏览器中访问上述地址" << std::endl;
        std::cout << "   Ctrl+C 退出服务器" << std::endl;
        std::cout << "==========================================" << std::endl;
        
        // 运行事件循环
        event_loop.Run();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 服务器异常: " << e.what() << std::endl;
        return 1;
    }
}
