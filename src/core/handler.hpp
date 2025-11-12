#pragma once

#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

#include "connection.hpp"


namespace ppsever {

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
    
    // 纯虚函数 - 定义必须实现的接口
    virtual void HandleRead(std::shared_ptr<Connection> conn) = 0;
    virtual void HandleWrite(std::shared_ptr<Connection> conn) = 0;
    virtual void HandleError(std::shared_ptr<Connection> conn) = 0;
    virtual void OnConnection(std::shared_ptr<Connection> conn) = 0;
    virtual void OnDisconnection(std::shared_ptr<Connection> conn) = 0;
    
    // 虚函数 - 可选的钩子函数
    virtual void SetNextHandler(std::shared_ptr<Handler> next) {
        next_handler_ = next;
    }
    
    // 模板方法 - 提供通用处理流程
    void ProcessRead(std::shared_ptr<Connection> conn) {
        // 前处理
        PreHandleRead(conn);
        
        // 具体处理（子类实现）
        HandleRead(conn);
        
        // 后处理
        PostHandleRead(conn);
        
        // 链式处理
        if (next_handler_) {
            next_handler_->ProcessRead(conn);
        }
    }
    
protected:
    // 受保护的构造函数 - 防止直接实例化
    Handler() = default;
    
    // 钩子函数 - 子类可选择重写
    virtual void PreHandleRead(std::shared_ptr<Connection> conn) { 
        // 默认空实现
    }
    
    virtual void PostHandleRead(std::shared_ptr<Connection> conn) { 
        // 默认空实现
    }
    
    // 禁止拷贝和移动
    Handler(const Handler&) = delete;
    Handler& operator=(const Handler&) = delete;
    Handler(Handler&&) = delete;
    Handler& operator=(Handler&&) = delete;
    
private:
    std::shared_ptr<Handler> next_handler_;  // 责任链模式
};

} // namespace ppsever