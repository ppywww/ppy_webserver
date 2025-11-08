#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include "core/connection.hpp"

namespace ppsever {

/**
 * ConnectionManager - 连接管理器类
 * 职责：统一管理所有TCP连接的生命周期，提供线程安全的连接操作
 * 特性：线程安全、资源自动释放、连接状态跟踪
 */
class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    // 禁止拷贝和移动
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) = delete;
    ConnectionManager& operator=(ConnectionManager&&) = delete;

    // 添加、删除、启动、停止连接
    void Start(std::shared_ptr<Connection> conn);
    void Stop(int fd);
    void StopAll();
    
    // 状态查询
    size_t GetCount() const;
    bool Exists(int fd) const;// 是否存在
    
    // 连接检索
    std::shared_ptr<Connection> GetConnection(int fd) const;

private:
    // 连接存储结构：文件描述符到Connection对象的映射
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    
    // 可变互斥锁：允许const方法修改锁状态
    mutable std::mutex connections_mutex_;
    
    // 统计计数器
    std::atomic<size_t> total_connections_{0};
};

} // namespace ppsever