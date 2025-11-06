#pragma once
struct DatabaseConfig {
    std::string host;
    uint16_t port;
    std::string username;
    std::string password;
    std::string database;
    size_t max_connections = 10;
    std::chrono::seconds timeout{5};
};

class ConnectionPool {
public:
    static ConnectionPool& GetInstance();
    
    bool Initialize(const DatabaseConfig& config);
    
    // 获取连接（RAII风格）
    std::shared_ptr<DatabaseConnection> GetConnection();
    
    // 统计信息
    size_t GetFreeConnectionCount() const;
    size_t GetActiveConnectionCount() const;
    
    void Shutdown();

private:
    class DatabaseConnection {
    public:
        bool Execute(const std::string& query);
        bool IsValid() const;
        void Release(); // 归还连接池
    };
};