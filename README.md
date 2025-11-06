

**PoolPoolServer** 是一个基于 **C++** 开发的高性能、模块化 Web 服务器。其核心设计理念是集成多种高效的**池化技术**（线程池、内存池、数据库连接池），旨在优化资源管理，显著提升服务器在高并发场景下的吞吐能力和响应速度。本项目既是学习网络编程与高性能服务设计的实践，也致力于提供一个可供生产环境参考的代码框架。



## 🌟 核心特性

*   **高性能网络模型**：基于 **I/O 多路复用**技术epoll，实现高并发连接管理。
*   **集成化池管理**：
    *   **线程池**：避免线程频繁创建与销毁的开销，实现任务的高效调度与执行。
    *   **内存池**：针对高频次、小对象的内存申请进行优化，减少内存碎片与系统调用开销。
    *   **连接池**：（规划中）统一管理数据库或外部服务连接，提升数据访问效率。
*   **可配置化**：支持通过配置文件灵活调整服务器参数（如端口、线程数量、池大小等）。
*   **跨平台支持**：代码设计考虑跨平台兼容性，可在 Linux、Windows 和 macOS 上编译运行。

## 🛠️ 技术栈

| 类别 | 技术/库 |
| :--- | :--- |
| **编程语言** | C++17 |
| **网络库** | 原生 Socket API / (可选: Boost.Asio) |
| **并发处理** | `std::thread`, `std::mutex`, `std::condition_variable` |
| **池化技术** | 自定义线程池、内存池、连接池实现 |
| **构建工具** | CMake |
| **操作系统** | Linux (主要开发环境)|

## 🚀 struct



### 前置条件

确保您的开发环境已安装以下工具：
*   **GCC** (版本 7.0+) 或 **Clang** (版本 5.0+)
*   **CMake** (版本 3.10+)


## ⚙️ 核心组件

### WebServer 核心
*   **描述**：负责服务器的启动、停止、网络监听、信号处理以及作为连接请求的入口。使用reactor模式实现。
*   **状态**：`开发中`
*   **关键特性**：事件循环、信号处理、优雅退出。

### 线程池 (Thread_Pool)
*   **描述**：基于生产者-消费者模型，管理一组工作线程，用于处理传入的HTTP请求等异步任务。
*   **状态**：`ing`
*   **配置参数**：核心线程数、最大线程数、任务队列容量、线程空闲存活时间。

### 内存池 (Memory_Pool)
*   **描述**：预分配一大块内存，并将其划分为多个固定大小或不同规格的小块，供程序快速申请和释放，尤其适用于高频小内存分配场景。
*   **状态**：`待开发`
*   **设计模式**：

### 数据库连接池 (Connection_Pool)
*   **描述**：管理到后端数据库MySQL连接，避免频繁建立和断开连接的开销。
*   **状态**：`规划中`
*   **功能**：连接复用、心跳保活、负载均衡。



### 使用线程池处理任务


## 📁 项目目录结构

```
ppsever/                          # 项目根目录
├── CMakeLists.txt               # 项目构建配置
├── README.md                    # 项目说明文档
├── config/                      # 配置文件目录
│   └── init.conf               # 初始化配置文件
├── docs/                        # 项目文档
├── examples/                    # 使用示例
├── include/                     # 公共头文件目录（目前为空）
├── src/                         # 源代码目录
│   ├── core/                   # 核心组件
│   │   ├── application.hpp     # 应用管理层
│   │   ├── event_loop.hpp      # 事件循环核心
│   │   ├── ppwebsever.cpp      # Web服务器实现
│   │   └── ppwebsever.hpp      # Web服务器接口
│   └── pool/                   # 池化组件模块
│       ├── connection_pool/    # 数据库连接池
│       │   ├── connection_pool.cpp
│       │   └── connection_pool.hpp
│       ├── memory_pool/        # 内存池
│       │   ├── memory_pool.cpp
│       │   └── memory_pool.hpp
│       └── thread_pool/        # 线程池
│           ├── core/          # 核心实现
│           │   ├── thread_pool.cpp
│           │   └── thread_pool.hpp
│           └── sample/        # 使用示例
│               └── pool_api.md
├── tests/                      # 测试代码
│   └── test.cpp
└── (其他文件)
```
## 🔧 开发指南 ##

🏗️ Application（应用程序总管）

application.hpp & application.cpp

角色：就像公司的总经理，负责整个服务器的启动、运行和关闭。

API功能：
• GetInstance()：获取总经理的唯一办公室（确保只有一个实例）

• Initialize()：总经理准备开业，检查所有设备是否就绪

• Run()：总经理下令正式营业，服务器开始接待客户

• Shutdown()：总经理下令打烊，优雅地关闭所有服务

• GetWebServer()等：总经理可以随时查看各个部门（如Web服务部）的工作状态

简单说：Application是大脑，控制整个服务器的生命周期的。

🌐 WebServer（网络服务部）

web_server.hpp & web_server.cpp

角色：就像公司的前台接待部门，专门处理客户（HTTP请求）的来访。

API功能：
• Start()：打开公司大门开始接待客户

• Stop()：礼貌地告诉客户"我们快打烊了"，等当前客户处理完

• ForceStop()：直接关门，不管还有没有客户在店里

• Get("/path", handler)：设置规则——当客户问"产品价格"时，由谁来回答案

• Post("/api", handler)：设置规则——当客户提交订单时，由谁来处理

• Use(middleware)：设置安检员，对所有客户进行统一检查（如身份验证）

简单说：WebServer是门面，直接与用户打交道，接收请求并返回响应。

🔄 EventLoop（事件调度中心）

event_loop.hpp & event_loop.cpp

角色：就像公司的调度员，不停查看有没有新客户来电或来访。

API功能：
• Loop()：调度员开始值班，不断检查电话和门铃

• Stop()：调度员下班

• AddFd(fd, events, callback)：告诉调度员"门铃响了就叫我"

• RunInLoop(task)：让调度员立即处理一个紧急任务

简单说：EventLoop是眼睛和耳朵，时刻监控所有活动连接。

👥 ThreadPool（员工团队）

thread_pool.hpp & thread_pool.cpp

角色：就像公司的员工团队，实际处理客户请求的工作人员。

API功能：
• Submit(task)：经理分配任务给空闲员工

• Start()：召集所有员工上岗

• Shutdown()：让员工们完成手头工作后下班

• SetCoreThreadSize(num)：调整常驻员工数量

简单说：ThreadPool是干活的人，真正处理业务逻辑。

💾 MemoryPool（物资管理处）

memory_pool.hpp & memory_pool.cpp

角色：就像公司的物资仓库，预先准备好常用物资，随用随取。

API功能：
• Construct()：从仓库领一套标准包装盒（内存块）

• Destroy()：把包装盒还回仓库以便重复使用

• Preallocate(num)：提前准备一批包装盒，避免临时短缺

简单说：MemoryPool是后勤保障，提高资源利用效率。

🔗 ConnectionPool（外联部）

connection_pool.hpp & connection_pool.cpp

角色：就像公司的外联专员，专门负责与数据库等外部系统打交道。

API功能：
• GetConnection()：派一个专员去数据库取数据

• Initialize(config)：设置外联专员的联系方式和工作流程

• Shutdown()：所有外联专员结束工作

简单说：ConnectionPool是桥梁，连接服务器和数据库。

🎯 如何协同工作

想象一个客户来访的场景：
1. 门卫（EventLoop）发现新客户敲门
2. 前台（WebServer）接待客户，了解需求
3. 经理（Application）决定处理流程
4. 后勤（MemoryPool）提供必要的工具和资源
5. 员工（ThreadPool）实际处理客户请求
6. 如果需要数据，外联（ConnectionPool）去数据库查询
7. 处理完成后，前台将结果返回给客户

💡 总结

每个API就像公司里的不同岗位，各司其职又相互配合：
• 管理岗（Application）负责整体协调

• 接待岗（WebServer）负责对外沟通

• 监控岗（EventLoop）负责实时调度

• 执行岗（ThreadPool）负责实际工作

• 后勤岗（MemoryPool）提供资源支持

• 外联岗（ConnectionPool）处理外部联系


**Happy Coding!** 如果您觉得这个项目有帮助，请给它一个 ⭐ Star 支持我们！