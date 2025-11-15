#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
// #include "connection_manager.hpp"

namespace ppserver {
class ThreadPool {
public:
    using Task = std::function<void()>;//uring引入std::function
    
    // 配置结构
    struct Config_thread_pool {
        size_t core_threads = 4;      // 核心线程数
        size_t max_threads = 16;      // 最大线程数
        size_t max_tasks = 1000;      // 任务队列容量
        std::chrono::seconds keep_alive_time{60}; // 空闲线程存活时间
        //std::chrono::seconds是std::chrono::duration的子类，用于表示一段时间，比如1秒、1分、1天等。
    };


    
    
    explicit ThreadPool(const Config_thread_pool& config );
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

private:
    bool CreateThread();

    // 数据成员
    Config_thread_pool config_;
    std::vector<std::thread> threads_;
    std::queue<Task> tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool shutdown_;
};

// 模板方法的实现必须放在头文件中
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;//uring声明
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 防止在关闭后提交新任务
        if (shutdown_) {
            throw std::runtime_error("ThreadPool is shutdown");
        }
        
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();//唤醒一个线程在队列中等待的任务
    return res;
}

} // namespace ppsever