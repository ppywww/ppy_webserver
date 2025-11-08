#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "http_request.hpp"

namespace ppsever {

/**
 * HTTP解析器状态枚举
 * 基于有限状态机模型，精确控制解析流程
 */
enum class ParseState {
    START_LINE,      // 解析请求行（方法、路径、版本）
    HEADERS,         // 解析头部字段
    BODY,           // 解析消息体
    CHUNKED_BODY,   // 解析分块传输编码的正文
    COMPLETE,       // 解析完成
    ERROR           // 解析错误
};

/**
 * HTTP解析结果结构
 * 提供详细的解析状态和错误信息
 */
struct ParseResult {
    bool success;                    // 解析是否成功
    ParseState state;               // 当前解析状态
    size_t bytes_parsed;           // 已解析字节数
    std::string error_message;     // 错误描述信息
    
    ParseResult() : success(false), state(ParseState::START_LINE), bytes_parsed(0) {}
};

/**
 * HTTP解析器主类
 * 实现HTTP/1.1协议解析的状态机
 */
class HttpParser {
public:
    HttpParser();
    ~HttpParser() = default;

    // 禁止拷贝和移动
    HttpParser(const HttpParser&) = delete;
    HttpParser& operator=(const HttpParser&) = delete;
    HttpParser(HttpParser&&) = delete;
    HttpParser& operator=(HttpParser&&) = delete;

    /**
     * 核心解析方法
     * @param data 输入数据指针
     * @param len 数据长度
     * @return 解析结果（包含状态和错误信息）
     */
    ParseResult Parse(const char* data, size_t len);
    
    /**
     * 获取解析完成的HTTP请求对象
     * @return 唯一指针指向HttpRequest，解析未完成时返回nullptr
     */
    std::unique_ptr<HttpRequest> GetRequest();
    
    /**
     * 重置解析器状态
     * 用于处理新请求或错误恢复
     */
    void Reset();
    
    /**
     * 检查是否正在解析中
     * @return 解析器是否处于活动状态
     */
    bool IsParsing() const;
    
    /**
     * 获取当前解析状态
     * @return 当前状态枚举值
     */
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