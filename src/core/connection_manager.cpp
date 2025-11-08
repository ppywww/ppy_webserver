#include "core/connection_manager.hpp"
#include <algorithm>
#include <iostream>

namespace ppsever {

ConnectionManager::ConnectionManager() {
    // 初始化时可以添加调试日志
    std::cout << "ConnectionManager initialized" << std::endl;
}

ConnectionManager::~ConnectionManager() {
    StopAll(); // 确保所有连接正确关闭
    std::cout << "ConnectionManager destroyed, total connections: " 
              << total_connections_ << std::endl;
}

void ConnectionManager::Start(std::shared_ptr<Connection> conn) {
    if (!conn) {
        return; // 空连接处理
    }
    int fd = conn->GetFd();
    if (fd < 0) {
        return; // 无效文件描述符
    }
    
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 检查是否已存在该连接
    if (connections_.find(fd) != connections_.end()) {
        std::cerr << "Connection already exists for FD: " << fd << std::endl;
        return;
    }
    
    // 添加新连接
    connections_[fd] = conn;
    total_connections_++;
    
    // 启动连接处理
    conn->Start();
    
    std::cout << "Connection started for FD: " << fd 
              << ", total: " << total_connections_ << std::endl;
}

void ConnectionManager::Stop(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        // 停止连接并移除
        it->second->Stop();
        connections_.erase(it);
        total_connections_--;
        
        std::cout << "Connection stopped for FD: " << fd 
                  << ", total: " << total_connections_ << std::endl;
    } else {
        std::cerr << "Connection not found for FD: " << fd << std::endl;
    }
}

void ConnectionManager::StopAll() {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (auto& pair : connections_) {
        pair.second->Stop(); // 停止每个连接
    }
    
    size_t stopped_count = connections_.size();
    connections_.clear();
    total_connections_ = 0;
    
    std::cout << "All connections stopped: " << stopped_count << std::endl;
}

size_t ConnectionManager::GetCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

bool ConnectionManager::Exists(int fd) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.find(fd) != connections_.end();
}

std::shared_ptr<Connection> ConnectionManager::GetConnection(int fd) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr; // 连接不存在
}

} // namespace ppsever