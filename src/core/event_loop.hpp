class EventLoop {
public:
    using Task = std::function<void()>;
    
    EventLoop();
    ~EventLoop();
    
    // 核心事件循环（阻塞调用）
    void Loop();
    
    // 安全停止循环（线程安全）
    void Stop();
    
    // 在当前事件循环线程中执行任务
    void RunInLoop(Task task);
    
    // 异步执行任务（线程安全）
    void QueueInLoop(Task task);
    
    // 文件描述符事件管理
    void AddChannel(Channel* channel);
    void UpdateChannel(Channel* channel);
    void RemoveChannel(int fd);
};

class Channel {
public:
    using EventCallback = std::function<void()>;
    
    Channel(EventLoop* loop, int fd);
    
    // 事件回调设置
    void SetReadCallback(EventCallback cb);
    void SetWriteCallback(EventCallback cb);
    void SetErrorCallback(EventCallback cb);
    
    // 事件监听控制
    void EnableReading();
    void EnableWriting();
    void DisableAll();
    
    int GetFd() const { return fd_; }
    uint32_t GetEvents() const { return events_; }
};