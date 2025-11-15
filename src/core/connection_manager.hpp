#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>

namespace ppserver {

// 前置声明
class Connection;

class ConnectionManager {
public:
    struct Config {
        size_t max_connections = 10000;  // 最大连接数
        int timeout_seconds = 30;        // 连接超时时间
    };

    struct Statistics {
        size_t active_connections = 0;   // 活跃连接数
        size_t total_connections = 0;    // 总连接数
    };

    ConnectionManager();
    ~ConnectionManager() = default;

    // 禁止拷贝和移动
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) = delete;
    ConnectionManager& operator=(ConnectionManager&&) = delete;

    // 添加连接
    bool AddConnection(int fd, std::shared_ptr<Connection> conn);
    
    // 移除连接
    void RemoveConnection(int fd);
    
    // 获取连接
    std::shared_ptr<Connection> GetConnection(int fd);
    
    // 获取统计信息
    Statistics GetStatistics();
    
    // 清理超时连接
    void CleanupTimeoutConnections();
    
    // 关闭所有连接
    void CloseAllConnections();

    bool IsPortAvailable(const std::string& , uint16_t ) ;
    


private:
    Config config_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    mutable std::mutex mutex_;  // 保护连接映射的互斥锁
};

} // namespace ppsever