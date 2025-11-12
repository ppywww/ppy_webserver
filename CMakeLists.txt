cmake_minimum_required(VERSION 3.10)
project(ppserver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器设置
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O2 -Wall -Wextra")

# 包含目录
include_directories(include)

# 核心源文件列表
set(CORE_SOURCES

    # src/core/connection_manager.cpp
    src/core/connection.cpp
    src/core/event_loop.cpp
    src/core/web_server.cpp

    src/core/http_response.cpp
    src/core/thread_pool.cpp
    src/core/http_parser.cpp
    src/core/http_request.cpp



    src/core/main.cpp
    
)

# 可执行文件
add_executable(ppserver ${CORE_SOURCES})

# 链接库
target_link_libraries(ppserver pthread)