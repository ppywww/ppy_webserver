


struct ServerConfig {
    std::string ip = "0.0.0.0";
    uint16_t port = 8080;
    int thread_num = 4;          // 线程池大小
    size_t max_connections = 10000;
};

class WebServer {
public:
    explicit WebServer(const ServerConfig& config);
    ~WebServer();
    
    // 启动服务器（非阻塞）
    bool Start();
    
    // 优雅停止
    void Stop();
    
    // 设置请求处理回调（业务逻辑入口）
    void SetRequestHandler(
        std::function<std::unique_ptr<HttpResponse>(std::unique_ptr<HttpRequest>)> handler
    );

private:
    // 内部事件回调
    void OnNewConnection(int sockfd);
    void OnDataReceived(int sockfd);
    void OnWriteCompleted(int sockfd);
};