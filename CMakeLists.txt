cmake_minimum_required(VERSION 3.10)

# 项目信息
project(PoolPoolServer VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/pool)
include_directories(${CMAKE_SOURCE_DIR}/src/pool/connection_pool)
include_directories(${CMAKE_SOURCE_DIR}/src/pool/memory_pool)
include_directories(${CMAKE_SOURCE_DIR}/src/pool/thread_pool/core)

# 查找所需的库
find_package(Threads REQUIRED)

# 定义源文件
set(CORE_SOURCES
    src/core/ppwebsever.cpp
)

set(POOL_SOURCES
    src/pool/thread_pool/core/thread_pool.cpp
    src/pool/memory_pool/memory_pool.cpp
    src/pool/connection_pool/connection_pool.cpp
)

# 创建核心库
add_library(ppwebserver_core STATIC ${CORE_SOURCES})

# 创建池库
add_library(ppserver_pools STATIC ${POOL_SOURCES})

# 链接依赖
target_link_libraries(ppserver_pools PRIVATE Threads::Threads)

# 创建可执行文件
add_executable(ppwebserver ${CORE_SOURCES} ${POOL_SOURCES})

# 链接库
target_link_libraries(ppwebserver PRIVATE Threads::Threads)

# 设置目标属性
set_target_properties(ppwebserver_core PROPERTIES
    VERSION ${PROJECT_VERSION}
)

set_target_properties(ppserver_pools PROPERTIES
    VERSION ${PROJECT_VERSION}
)

# 安装规则
install(TARGETS ppwebserver
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/ppwebserver
    FILES_MATCHING PATTERN "*.conf"
)

install(DIRECTORY examples/HTML/
    DESTINATION share/ppwebserver/html
    FILES_MATCHING PATTERN "*.html"
)

# 启用测试
enable_testing()

# 添加测试可执行文件
add_executable(http_client_test tests/HTTPClient.cpp)
target_link_libraries(http_client_test PRIVATE Threads::Threads)
add_test(NAME HTTPClientTest COMMAND http_client_test)