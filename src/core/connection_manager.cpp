#include "connection_manager.hpp"
#include "connection.hpp"
#include <algorithm>
#include <ctime>

namespace ppserver {

ConnectionManager::ConnectionManager() = default;

bool ConnectionManager::AddConnection(int fd, std::shared_ptr<Connection> conn) {
    if (!conn) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否超过最大连接数限制
    if (connections_.size() >= config_.max_connections) {
        return false;
    }

    connections_[fd] = conn;
    return true;
}

void ConnectionManager::RemoveConnection(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(fd);
}

std::shared_ptr<Connection> ConnectionManager::GetConnection(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

ConnectionManager::Statistics ConnectionManager::GetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    Statistics stats;
    stats.active_connections = connections_.size();
    stats.total_connections = connections_.size(); // 在实际实现中可能需要跟踪总连接数
    return stats;
}

void ConnectionManager::CleanupTimeoutConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    time_t current_time = time(nullptr);
    
    for (auto it = connections_.begin(); it != connections_.end();) {
        auto conn = it->second;
        if (conn && (current_time - conn->GetLastActivityTime()) > config_.timeout_seconds) {
            conn->Close();
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }
}

void ConnectionManager::CloseAllConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : connections_) {
        if (pair.second) {
            pair.second->Close();
        }
    }
    
    connections_.clear();
}

bool ConnectionManager::IsPortAvailable(const std::string& host, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    bool available = (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    close(sock);
    return available;
}


} // namespace ppsever