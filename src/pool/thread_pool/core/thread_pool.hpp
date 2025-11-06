class ThreadPool {
public:
    using Task = std::function<void()>;
    
    // 配置结构
    struct Config {
        size_t core_threads = 4;      // 核心线程数
        size_t max_threads = 16;      // 最大线程数
        size_t max_tasks = 1000;      // 任务队列容量
        std::chrono::seconds keep_alive_time{60}; // 空闲线程存活时间
    };
    
    explicit ThreadPool(const Config& config);
    ~ThreadPool();
    
    // 提交任务，返回future获取结果
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // 动态调整线程数
    bool SetCoreThreadSize(size_t num);
    bool SetMaxThreadSize(size_t num);
    
    // 状态查询
    size_t GetPendingTaskCount() const;
    size_t GetActiveThreadCount() const;
    
    // 优雅关闭
    void Shutdown(bool wait_for_completion = true);
};