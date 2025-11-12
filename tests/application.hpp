#pragma once

namespace ppsever {

class Application {
public:
    static Application& GetInstance();
    
    bool Initialize(int argc, char** argv);
    int Run();
    void Shutdown();
    
    // 获取各组件实例
    class WebServer* GetWebServer() const;
    class ThreadPool* GetThreadPool() const;
    // class EventLoop* GetEventLoop() const;
};

} // namespace ppsever