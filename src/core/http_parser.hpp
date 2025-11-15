#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include<unordered_set>
#include "http_request.hpp"

namespace ppserver {


enum class ParseState {
    START_LINE,      // 解析请求行（方法、路径、版本）
    HEADERS,         // 解析头部字段
    BODY,           // 解析消息体
    CHUNKED_BODY,   // 解析分块传输编码的正文
    COMPLETE,       // 解析完成
    ERROR           // 解析错误
};


struct ParseResult {
    bool success;                    // 解析是否成功
    ParseState state;               // 当前解析状态
    size_t bytes_parsed;           // 已解析字节数
    std::string error_message;     // 错误描述信息
    
    ParseResult() : success(false), state(ParseState::START_LINE), bytes_parsed(0) {}
};


class HttpParser {
public:
    HttpParser();
    ~HttpParser() = default;

    // 禁止拷贝和移动
    HttpParser(const HttpParser&) = delete;
    HttpParser& operator=(const HttpParser&) = delete;
    HttpParser(HttpParser&&) = delete;
    HttpParser& operator=(HttpParser&&) = delete;
    ParseResult Parse(const char* data, size_t len);
    std::unique_ptr<HttpRequest> GetRequest();
    
  
    void Reset();
    
 
    bool IsParsing() const;
    
    ParseState GetCurrentState() const;

private:
    // 解析阶段处理方法
    ParseResult ParseStartLine(const char* data, size_t len, size_t& pos);
    ParseResult ParseHeaders(const char* data, size_t len, size_t& pos);
    ParseResult ParseBody(const char* data, size_t len, size_t& pos);
    ParseResult ParseChunkedBody(const char* data, size_t len, size_t& pos);
    
    // 辅助方法
    bool CheckComplete() const;
    size_t FindCRLF(const char* data, size_t len, size_t start_pos) const;
    bool ValidateHttpVersion(const std::string& version) const;
    bool ValidateHttpMethod(const std::string& method) const;
    
    // 状态转换处理
    void TransitionTo(ParseState new_state);
    void HandleError(const std::string& message);
    
    // 成员变量
    ParseState state_;                      // 当前解析状态
    std::unique_ptr<HttpRequest> request_;  // 正在构建的请求对象
    size_t content_length_;                 // 内容长度（用于定长正文）
    bool chunked_encoding_;                 // 是否分块传输编码
    std::string buffer_;                    // 临时缓冲区（用于跨数据包解析）
    
    // 解析统计
    size_t total_bytes_parsed_;             // 总解析字节数
    size_t current_chunk_size_;              // 当前分块大小
    bool chunk_size_parsed_;                // 分块大小是否已解析
};

} // namespace ppsever