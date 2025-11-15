#pragma once

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

#include"web_server.hpp"
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "connection_manager.hpp"
#include "connection.hpp"


namespace ppserver {

// 前置声明
class Connection;

/**
 * Handler - 抽象处理器基类
 * 设计模式：模板方法模式 + 策略模式
 */
class Handler {
public:
    // 虚析构函数 - 确保正确的多态销毁
    virtual ~Handler() = default;
    
    // 构造函数
    Handler(EventLoop& loop, Connection& conn, ThreadPool& thread_pool);
    
    // 纯虚函数 - 定义必须实现的接口
     void HandleRead(std::shared_ptr<Connection> conn) ;
     void HandleWrite(std::shared_ptr<Connection> conn) ;
     void HandleError(std::shared_ptr<Connection> conn) ;

     void OnConnection(std::shared_ptr<Connection> conn) ;
    void OnDisconnection(std::shared_ptr<Connection> conn);
    
    // 虚函数 - 可选的钩子函数
    void SetNextHandler(std::shared_ptr<Handler> next) {
        next_handler_ = next;
    }
    
protected:
    // 受保护的成员变量
    EventLoop& loop_;
    Connection& connection_;
    ThreadPool& thread_pool_;
    
    // 受保护的构造函数 - 防止直接实例化
protected:
    std::shared_ptr<Handler> next_handler_;
};



} // namespace ppserver