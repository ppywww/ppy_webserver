#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <queue>

namespace ppsever {

/**
 * EventLoop - 事件循环核心组件
 * 负责：I/O事件多路复用、定时器管理、跨线程任务调度
 * 设计特点：单线程事件循环、边缘触发模式、最小堆定时器
 */
class EventLoop {
public:
    // 事件标志常量
    static constexpr uint32_t EPOLL_READ = EPOLLIN | EPOLLPRI;    // 读事件
    static constexpr uint32_t EPOLL_WRITE = EPOLLOUT;             // 写事件  
    static constexpr uint32_t EPOLL_ERROR = EPOLLERR | EPOLLHUP;  // 错误事件
    static constexpr uint32_t EPOLL_ET = EPOLLET;                 // 边缘触发模式

    // 类型别名
    using EventCallback = std::function<void(int fd, uint32_t events)>;
    using Task = std::function<void()>;
    using TimerId = uint64_t;

    EventLoop();
 
    ~EventLoop()
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    // 核心控制接口
    int Run();
    void Stop();
    bool IsInLoopThread() const;

    // 文件描述符管理  将所有对同一个 epfd 的 epoll_ctl 操作，串行化到同一个线程（通常是事件循环线程）执行，避免多线程直接调用 epoll_ctl。
    void AddFd(int fd, uint32_t events, EventCallback callback);
    void UpdateFd(int fd, uint32_t events);
    void RemoveFd(int fd);

    // 定时器接口
    TimerId RunAfter(uint64_t delay_ms, Task callback);
    TimerId RunEvery(uint64_t interval_ms, Task callback);
    void CancelTimer(TimerId timer_id);

    // 任务调度接口
    void RunInLoop(Task task);// 在事件循环线程中执行任务
    void QueueInLoop(Task task);// 在线程安全队列中添加任务，稍后执行

    // 性能监控接口
    struct Statistics {
        size_t active_fd_count;      // 监控中的FD数量
        size_t pending_tasks;         // 待处理任务数
        size_t active_timers;         // 活跃定时器数
        uint64_t loop_iterations;     // 事件循环迭代次数
    };
    Statistics GetStatistics() const;

private:
    // 定时器结构
    struct Timer {
        TimerId id;
        uint64_t expiration;          // 到期时间戳(毫秒)
        uint64_t interval;           // 重复间隔(毫秒)
        Task callback;
        bool repeated;               // 是否重复执行

        // 最小堆比较函数
        struct Compare {
            bool operator()(const Timer& a, const Timer& b) const {
                return a.expiration > b.expiration;
            }
        };
    };

    // 辅助方法
    uint64_t GetCurrentTimeMs() const;
    int CalculateNextTimeout() const;
    void ProcessExpiredTimers();// 处理到期定时器
    void ProcessPendingTasks();// 处理待执行任务
    void HandleTaskNotification();// 处理任务通知事件
    void WakeUp();// 唤醒事件循环
    void HandleIoEvent(const epoll_event& event);// 处理I/O事件

    // 成员变量
    int epoll_fd_;                   // epoll实例文件描述符
    int event_fd_;                   // 事件通知文件描述符
    std::atomic<bool> running_;      // 运行状态标志
    std::thread::id owner_thread_id_; // 所属线程ID

    // 文件描述符回调映射
    std::unordered_map<int, EventCallback> fd_callbacks_;
    mutable std::mutex fd_mutex_;     // FD映射的互斥锁

    // 定时器队列（最小堆）
    std::vector<Timer> timers_;
    mutable std::mutex timer_mutex_;  // 定时器队列的互斥锁
    std::atomic<TimerId> next_timer_id_; // 定时器ID生成器

    // 任务队列
    std::vector<Task> pending_tasks_;//
    mutable std::mutex task_mutex_;   // 任务队列的互斥锁
};

} // namespace ppsever