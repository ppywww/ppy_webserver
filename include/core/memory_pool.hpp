#pragma once

template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t chunk_size = 1024);
    ~MemoryPool();
    
    // 分配/释放对象内存
    template<typename... Args>
    T* Construct(Args&&... args);
    
    void Destroy(T* ptr);
    
    // 批量预分配（提升性能）
    void Preallocate(size_t num_objects);
    
    // 统计信息
    size_t GetFreeCount() const;
    size_t GetUsedCount() const;
};

// HTTP对象特化内存池
using HttpRequestPool = MemoryPool<HttpRequest>;
using HttpResponsePool = MemoryPool<HttpResponse>;