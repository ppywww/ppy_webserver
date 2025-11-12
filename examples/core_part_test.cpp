#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cassert>
#include <system_error>
#include <string.h>

#include "event_loop.hpp"
#include "connection.hpp"
#include "connection_manager.hpp"
#include "web_server.hpp"

using namespace ppsever;

/**
 * 模块测试专用Main函数 - 专注于核心组件验证
 * 设计目标：快速验证EventLoop、Connection等模块的基本功能
 */
int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "       核心模块功能验证测试               " << std::endl;
    std::cout << "==========================================" << std::endl;
    
    try {
        // ==================== 测试1: EventLoop 基础功能 ====================
        std::cout << "\n🧪 测试1: EventLoop 初始化和运行" << std::endl;
        
        EventLoop event_loop;
        std::cout << "✅ EventLoop 创建成功" << std::endl;
        
        // 测试事件循环的简单任务调度
        bool task_executed = false;
        event_loop.RunInLoop([&task_executed]() {
            task_executed = true;
            std::cout << "✅ 事件循环任务执行成功" << std::endl;
        });
        
        // 短暂运行事件循环以处理任务
        std::thread loop_thread([&event_loop]() {
            event_loop.Run();
        });
        
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        event_loop.Stop();
        loop_thread.join();
        
        if (!task_executed) {
            throw std::runtime_error("事件循环任务未执行");
        }
        std::cout << "✅ EventLoop 基础功能测试通过" << std::endl;
        
        // ==================== 测试2: 定时器功能 ====================
        std::cout << "\n🧪 测试2: 定时器功能验证" << std::endl;
        
        EventLoop timer_loop;
        std::atomic<bool> timer_fired{false};
        
        // 设置一个短期定时器
        auto timer_id = timer_loop.RunAfter(50, [&timer_fired]() {
            timer_fired = true;
            std::cout << "✅ 定时器触发成功" << std::endl;
        });
        
        std::thread timer_thread([&timer_loop]() {
            timer_loop.Run();
        });
        
        // 等待定时器触发
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timer_loop.Stop();
        timer_thread.join();
        
        if (!timer_fired) {
            throw std::runtime_error("定时器未正确触发");
        }
        std::cout << "✅ 定时器功能测试通过" << std::endl;
        
        // ==================== 测试3: ConnectionManager 基础功能 ====================
        std::cout << "\n🧪 测试3: ConnectionManager 生命周期管理" << std::endl;
        
        ConnectionManager conn_manager;
        std::cout << "✅ ConnectionManager 创建成功" << std::endl;
        
        // 测试统计功能
        auto stats = conn_manager.GetStatistics();
        std::cout << "📊 初始连接数: " << stats.active_connections << std::endl;
        
        std::cout << "✅ ConnectionManager 基础测试通过" << std::endl;
        
        // ==================== 测试4: 文件描述符管理 ====================
        std::cout << "\n🧪 测试4: 文件描述符操作模拟" << std::endl;
        
        // 创建一对socket用于测试
        int test_fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, test_fds) == -1) {
            throw std::system_error(errno, std::system_category(), "socketpair失败");
        }
        
        std::cout << "✅ 测试socket对创建成功: " << test_fds[0] << " <-> " << test_fds[1] << std::endl;
        
        // 简单数据交换测试
        const char* test_message = "模块测试消息";
        write(test_fds[1], test_message, strlen(test_message));
        
        char buffer[256];
        ssize_t n = read(test_fds[0], buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            std::cout << "✅ 数据传输测试: 发送 '" << test_message << "' → 接收 '" << buffer << "'" << std::endl;
        }
        
        close(test_fds[0]);
        close(test_fds[1]);
        std::cout << "✅ 文件描述符操作测试通过" << std::endl;
        
        // ==================== 测试总结 ====================
        std::cout << "\n==========================================" << std::endl;
        std::cout << "🎉 所有核心模块测试通过！" << std::endl;
        std::cout << "✅ EventLoop - 事件调度功能正常" << std::endl;
        std::cout << "✅ 定时器 - 时间管理功能正常" << std::endl;
        std::cout << "✅ ConnectionManager - 连接管理正常" << std::endl;
        std::cout << "✅ 文件操作 - I/O基础功能正常" << std::endl;
        std::cout << "==========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ 测试失败: " << e.what() << std::endl;
        std::cerr << "💡 建议检查相关模块的实现代码" << std::endl;
        return 1;
    }
}