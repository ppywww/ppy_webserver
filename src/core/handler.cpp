#include "handler.hpp"
#include <system_error>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>

namespace ppserver {

// 构造函数
Handler::Handler(EventLoop& loop, Connection& conn, ThreadPool& thread_pool)
    : loop_(loop),
      connection_(conn),
      thread_pool_(thread_pool) {
}

// HTTP处理器方法实现

void Handler::HandleRead(std::shared_ptr<Connection> conn) {
    std::cout << "Handling HTTP request from: " << conn->GetRemoteAddress() << std::endl;
    
    // 读取数据
    ssize_t bytes_read = conn->ReadData();
    if (bytes_read < 0) {
        std::cerr << "Failed to read data from client" << std::endl;
        conn->Close();
        return;
    }
    
    if (bytes_read == 0) {
        // 客户端关闭连接
        conn->Close();
        return;
    }
    
    // 获取读取的数据
    std::string requestData = conn->GetReadBuffer();
    
    
    // 检查是否收到完整的HTTP请求（以\r\n\r\n结尾）
    if (requestData.find("\r\n\r\n") != std::string::npos) {
        // 构建HTTP响应
        std::cout << "Received data: " << requestData << std::endl;
        
        std::string content = "<h1>Hello PP</h1>";
    
        std::string response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n" // 添加字符集
        "Content-Length: " + std::to_string(content.length()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n" // 添加CORS头
        "\r\n" // 空行分隔头部和主体
        + content;
        
        std::cout << "Sending response to client" << std::endl;
        conn->WriteData(response);
        
        // 设置写回调，在写完成后关闭连接
       conn->ClearReadBuffer();
    }
    
}


void Handler::HandleWrite(std::shared_ptr<Connection> conn) {
    // 使用默认处理
    conn->DefaultHandleWrite();
}

void Handler::HandleError(std::shared_ptr<Connection> conn) {
    // 使用默认处理
    conn->DefaultHandleError();
}

void Handler::OnConnection(std::shared_ptr<Connection> conn) {
    std::cout << "New HTTP connection from: " << conn->GetRemoteAddress() << std::endl;
}

void Handler::OnDisconnection(std::shared_ptr<Connection> conn) {
    std::cout << "HTTP connection closed: " << conn->GetRemoteAddress() << std::endl;
}

} // namespace ppserver