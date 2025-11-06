namespace poolpoolserver {

class Application {
public:
    // 初始化所有组件：WebServer、ThreadPool、MemoryPool、ConnectionPool
    static bool Initialize(const ServerConfig& config);
    
    // 启动事件循环，阻塞直到收到停止信号
    static void Run();
    
    // 优雅停止，等待所有任务完成
    static void Shutdown();
    
    // 获取各组件实例（用于自定义扩展）
    static WebServer* GetWebServer();
    static ThreadPool* GetThreadPool();
    static MemoryPool<HttpRequest>* GetRequestPool();
    static MemoryPool<HttpResponse>* GetResponsePool();
    static ConnectionPool* GetDatabasePool();
};

} // namespace poolpoolserver