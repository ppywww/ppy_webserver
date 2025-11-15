#include "event_loop.hpp"
#include <system_error>
#include <fcntl.h>
#include <cstring>
#include <algorithm> 
#include <iostream>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include <functional>     // 用于 std::function
#include <unistd.h>

namespace ppserver {

EventLoop::EventLoop() 
    : epoll_fd_(-1),
      event_fd_(-1),
      running_(false),
      next_timer_id_(1) {
    
    // 创建epoll实例
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);//EPOLL_CLOEXEC确保子进程不会继承该文件描述符
    if (epoll_fd_ < 0) {
        throw std::runtime_error("Failed to create epoll instance: " + 
                               std::string(strerror(errno)));
    }
    
    // 创建eventfd用于任务通知
    event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0) {
        close(epoll_fd_);
        throw std::runtime_error("Failed to create eventfd: " + 
                               std::string(strerror(errno)));
    }
    
    // 注册eventfd到epoll监控
    epoll_event event{};//
    event.events = EPOLL_READ;
    event.data.fd = event_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &event) < 0) {
        close(epoll_fd_);
        close(event_fd_);
        throw std::runtime_error("Failed to add eventfd to epoll: " + 
                               std::string(strerror(errno)));
    }
}

EventLoop::~EventLoop() {
    Stop();
    if (epoll_fd_ >= 0) close(epoll_fd_);
    if (event_fd_ >= 0) close(event_fd_);
}

int EventLoop::Run() {
    if (running_) {
        return 0;
    }
    
    running_ = true;
    owner_thread_id_ = std::this_thread::get_id();
    
    const int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];
    
    // 主事件循环
    while (running_) {
        // 计算最近定时器到期时间
        int timeout = CalculateNextTimeout();
        
        // 等待事件或超时
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout);

        if (num_events < 0 && errno != EINTR) {
            // 非中断性错误，记录并继续
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            continue;
        }
        
        // 处理I/O事件
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == event_fd_) {//如果fd==event_fd_,说明是任务通知事件
                HandleTaskNotification(); // 内部任务通知
            } else {
                HandleIoEvent(events[i]); // 外部I/O事件
            }
        }
        
        // 处理到期定时器
        ProcessExpiredTimers();
        
        // 执行待处理任务
        ProcessPendingTasks();
    }
    
    return 0;
}

void EventLoop::Stop() {
    running_ = false;
    WakeUp(); // 唤醒eventfd事件 让epoll_wait返回：检测到eventfd可读事件，立即从阻塞状态返回 状态检查：循环回到while (running_)条件检查
}

bool EventLoop::IsInLoopThread() const {
    return owner_thread_id_ == std::this_thread::get_id();
}

void EventLoop::AddFd(int fd, uint32_t events, EventCallback callback) {
    
    std::lock_guard<std::recursive_mutex> lock(fd_mutex_);//保护共享资源 fd_callbacks_（文件描述符回调映射）的线程安全访问
    
    // 设置边缘触发模式
    events |= EPOLL_ET;//uint32_t类型的位掩码（bitmask），用于指定要监控的事件类型
    
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        throw std::runtime_error("Failed to add fd to epoll: " + 
                               std::string(strerror(errno)));
    }
    
    fd_callbacks_[fd] = std::move(callback);


}
//==================================重入锁问题 使用递归锁==================================
// void EventLoop::AddFd(int fd, uint32_t events, EventCallback callback) {
//     std::cout << "🔄 进入 AddFd，线程ID: " << std::this_thread::get_id() 
//               << ", 文件描述符: " << fd << std::endl;
    
//     auto start_time = std::chrono::steady_clock::now();
    
    
//     {
//         std::cout << "🔒 尝试获取 fd_mutex_..." << std::endl;
//         std::lock_guard<std::recursive_mutex> lock(fd_mutex_);
//         std::cout << "✅ 成功获取 fd_mutex_" << std::endl;
        
//         // 设置边缘触发模式
//         events |= EPOLL_ET;
        
//         epoll_event event{};
//         event.events = events;
//         event.data.fd = fd;
        
//         std::cout << "🔧 准备调用 epoll_ctl，epoll_fd: " << epoll_fd_ 
//                   << ", 目标fd: " << fd << std::endl;
        
//         if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
//             std::string error_msg = "Failed to add fd to epoll: " + std::string(strerror(errno));
//             std::cerr << "❌ " << error_msg << std::endl;
//             throw std::runtime_error(error_msg);
//         }
        
//         std::cout << "✅ epoll_ctl 调用成功" << std::endl;
        
//         fd_callbacks_[fd] = std::move(callback);
//         std::cout << "✅ 回调函数注册成功" << std::endl;
   
//     }
    
//     auto end_time = std::chrono::steady_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
//     std::cout << "⏱️  AddFd 完成，耗时: " << duration.count() << "ms" << std::endl;
// }

void EventLoop::UpdateFd(int fd, uint32_t events) {
    epoll_event event{};
    event.events = events | EPOLL_ET; // 保持边缘触发
    event.data.fd = fd;
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) < 0) {
        throw std::runtime_error("Failed to update fd in epoll: " + 
                               std::string(strerror(errno)));
    }
}

void EventLoop::RemoveFd(int fd) {
    std::lock_guard<std::recursive_mutex> lock(fd_mutex_);
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // 记录警告但继续执行（可能fd已关闭）
        std::cerr << "Warning: Failed to remove fd from epoll: " 
                  << strerror(errno) << std::endl;
    }
    
    fd_callbacks_.erase(fd);
}


EventLoop::TimerId EventLoop::RunAfter(uint64_t delay_ms, Task callback) {//返回类型为 TimerId，即定时器的唯一标识符
    //传入delay_ms参数
    std::lock_guard<std::mutex> lock(timer_mutex_);
   

        Timer timer;
    timer.id = next_timer_id_++;
    timer.expiration = GetCurrentTimeMs() + delay_ms;//计算过期时间
    timer.interval = 0;//间隔时间是0，表示一次性定时器
    timer.callback = std::move(callback);
    timer.repeated = false;//间隔时间是0，表示一次性定时器 那就不会重复执行
    
    timers_.push_back(timer);
    std::push_heap(timers_.begin(), timers_.end(), Timer::Compare());
    
    WakeUp(); // 唤醒事件循环重新计算超时
    return timer.id;
}

EventLoop::TimerId EventLoop::RunEvery(uint64_t interval_ms, Task callback) {//可以用来执行心跳检测任务
    std::lock_guard<std::mutex> lock(timer_mutex_);
    
    Timer timer;
    timer.id = next_timer_id_++;//生成唯一定时器ID
    timer.expiration = GetCurrentTimeMs() + interval_ms;//过期时间
    timer.interval = interval_ms;//间隔时间
    timer.callback = std::move(callback);//回调函数
    timer.repeated = true;//是否重复执行标志
    
    timers_.push_back(timer);
    std::push_heap(timers_.begin(), timers_.end(), Timer::Compare());
    
    WakeUp(); // 唤醒事件循环重新计算超时
    return timer.id;
}

void EventLoop::CancelTimer(TimerId timer_id) {//取消定时器 在最小堆里删除
    std::lock_guard<std::mutex> lock(timer_mutex_);
    
    auto it = std::find_if(timers_.begin(), timers_.end(),
        [timer_id](const Timer& timer) { return timer.id == timer_id; });
    
    if (it != timers_.end()) {
        timers_.erase(it);
        std::make_heap(timers_.begin(), timers_.end(), Timer::Compare());
    }
}

void EventLoop::RunInLoop(Task task) {
    if (IsInLoopThread()) {//当前线程是事件循环线程 如果是，则直接执行任务 不是则加入队列异步执行 这个队列是线程安全的 加入队列会先枷锁
        task(); // 直接在当前线程执行
    } else {
        QueueInLoop(std::move(task)); // 加入队列异步执行
    }
}

void EventLoop::QueueInLoop(Task task) {
    {
        std::lock_guard<std::recursive_mutex> lock(task_mutex_);
        pending_tasks_.push_back(std::move(task));
    }
    WakeUp(); // 唤醒事件循环处理新任务
}

EventLoop::Statistics EventLoop::GetStatistics() const {
    Statistics stats;
    {
        std::lock_guard<std::recursive_mutex> lock(fd_mutex_);//保护fd_callbacks_的互斥锁
        //将所有对同一个 epfd 的 epoll_ctl 操作，串行化到同一个线程（通常是事件循环线程）执行，避免多线程直接调用 epoll_ctl。
        stats.active_fd_count = fd_callbacks_.size();
    }
    {
        std::lock_guard<std::recursive_mutex> lock(task_mutex_);//保护任务队列的互斥锁
        stats.pending_tasks = pending_tasks_.size();
    }
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);//最小堆的
        stats.active_timers = timers_.size();
    }
    return stats;
}

// ==================== 私有辅助方法实现 ====================

uint64_t EventLoop::GetCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

int EventLoop::CalculateNextTimeout() const {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    
    if (timers_.empty()) {
        return -1; // 无定时器，无限等待
    }
    
    uint64_t now = GetCurrentTimeMs();
    uint64_t next_expire = timers_.front().expiration;
    
    if (next_expire <= now) {
        return 0; // 有定时器已到期，立即处理
    }
    
    return static_cast<int>(next_expire - now); // 返回精确等待时间
}

void EventLoop::ProcessExpiredTimers() {
    std::vector<Timer> expired_timers;
    uint64_t now = GetCurrentTimeMs();
    
    // 批量取出所有到期定时器
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        while (!timers_.empty() && timers_.front().expiration <= now) {
            expired_timers.push_back(timers_.front());
            std::pop_heap(timers_.begin(), timers_.end(), Timer::Compare());
            timers_.pop_back();
        }
    }
    
    // 执行到期定时器回调
    for (auto& timer : expired_timers) {
        try {
            timer.callback();
        } catch (const std::exception& e) {
            std::cerr << "Timer callback error: " << e.what() << std::endl;
        }
        
        // 重复定时器重新加入队列
        if (timer.repeated) {
            std::lock_guard<std::mutex> lock(timer_mutex_);
            timer.expiration = now + timer.interval;
            timers_.push_back(timer);
            std::push_heap(timers_.begin(), timers_.end(), Timer::Compare());
        }
    }
}

void EventLoop::ProcessPendingTasks() {
    std::vector<Task> tasks;
    {
        std::lock_guard<std::recursive_mutex> lock(task_mutex_);
        tasks.swap(pending_tasks_); // 批量取出所有任务
    }
    
    for (auto& task : tasks) {
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "Task execution error: " << e.what() << std::endl;
        }
    }
}

void EventLoop::HandleTaskNotification() {
    uint64_t value;
    // 读取eventfd值（清空通知）
    if (read(event_fd_, &value, sizeof(value)) < 0 && errno != EAGAIN) {
        std::cerr << "Failed to read from eventfd: " << strerror(errno) << std::endl;
    }
}

void EventLoop::WakeUp() {
    uint64_t value = 1;
    // 写入eventfd触发通知
    if (write(event_fd_, &value, sizeof(value)) < 0 && errno != EAGAIN) {
        std::cerr << "Failed to write to eventfd: " << strerror(errno) << std::endl;
    }
}

void EventLoop::HandleIoEvent(const epoll_event& event) {
    std::lock_guard<std::recursive_mutex> lock(fd_mutex_);
    auto it = fd_callbacks_.find(event.data.fd);
    if (it != fd_callbacks_.end()) {
        try {
            it->second(event.data.fd, event.events); // 执行注册的回调
        } catch (const std::exception& e) {
            std::cerr << "IO event callback error: " << e.what() << std::endl;
        }
    }
}

} // namespace ppsever