#pragma once

namespace ppsever {

class EventLoop {
public:
    using Task = std::function<void()>;
    
    EventLoop();
    ~EventLoop();
    
    void Loop();
    void Stop();
    void RunInLoop(Task task);
    
    // 文件描述符管理
    void AddFd(int fd, uint32_t events);
    void RemoveFd(int fd);
};

} // namespace ppsever