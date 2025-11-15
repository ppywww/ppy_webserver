#include "thread_pool.hpp"
#include <iostream>

namespace ppserver {




ThreadPool::ThreadPool(const Config_thread_pool& config ) 
    : config_(config), 
      shutdown_(false) {
    
    // 创建核心线程数
    for (size_t i = 0; i < config_.core_threads; ++i) {
        CreateThread();
    }
}

ThreadPool::~ThreadPool() {
    Shutdown(true);
}

bool ThreadPool::CreateThread() {
    try {
        threads_.emplace_back([this]() {
            while (true) {
                Task task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { 
                        return shutdown_ || !tasks_.empty(); 
                    });
                    
                    if (shutdown_ && tasks_.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                task();
            }
        });
        
        return true;
    } catch (...) {
        return false;
    }
}

void ThreadPool::Shutdown(bool wait_for_completion) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (shutdown_) {
            return;
        }
        shutdown_ = true;
    }
    
    condition_.notify_all();
    
    if (wait_for_completion) {
        for (std::thread& worker : threads_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
}

size_t ThreadPool::GetPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

size_t ThreadPool::GetActiveThreadCount() const {
    return threads_.size();
}

// bool ThreadPool::SetCoreThreadSize(size_t num) {
//     // 简化实现，实际项目中需要更复杂的线程管理逻辑
//     return true;
// }

// bool ThreadPool::SetMaxThreadSize(size_t num) {
//     // 简化实现，实际项目中需要更复杂的线程管理逻辑
//     return true;
// }

} // namespace ppsever