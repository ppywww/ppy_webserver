#include "http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

namespace ppserver {

HttpParser::HttpParser() 
    : state_(ParseState::START_LINE),
      content_length_(0),
      chunked_encoding_(false),
      total_bytes_parsed_(0),
      current_chunk_size_(0),
      chunk_size_parsed_(false) {
    
    request_ = std::make_unique<HttpRequest>();
}

// ... existing code ...
ParseResult HttpParser::Parse(const char* data, size_t len) {
    if (len == 0 || data == nullptr) {
        ParseResult result;
        result.success = false;
        result.state = state_;
        result.bytes_parsed = total_bytes_parsed_;
        result.error_message = "Invalid input data";
        return result;
    }
    
    // 将新数据追加到缓冲区（处理跨数据包的情况）
    buffer_.append(data, len);
    
    size_t pos = 0;
    ParseResult result;
    result.bytes_parsed = 0;
    
    try {
        // 基于当前状态进行解析
        while (pos < buffer_.size() && state_ != ParseState::COMPLETE && state_ != ParseState::ERROR) {
            size_t prev_pos = pos;
            
            switch (state_) {
                case ParseState::START_LINE:
                    result = ParseStartLine(buffer_.c_str(), buffer_.size(), pos);
                    break;
                case ParseState::HEADERS:
                    result = ParseHeaders(buffer_.c_str(), buffer_.size(), pos);
                    break;
                case ParseState::BODY:
                    result = ParseBody(buffer_.c_str(), buffer_.size(), pos);
                    break;
                case ParseState::CHUNKED_BODY:
                    result = ParseChunkedBody(buffer_.c_str(), buffer_.size(), pos);
                    break;
                default:
                    HandleError("Invalid parser state");
                    break;
            }
            
            if (!result.success) {
                return result;
            }
            
            // 更新已处理位置
            result.bytes_parsed += (pos - prev_pos);
            total_bytes_parsed_ += (pos - prev_pos);
        }
        
        // 检查是否完成解析
        if (state_ == ParseState::COMPLETE) {
            result.success = true;
            result.state = state_;
        }
        
        // 清理已处理的数据
        if (pos > 0) {
            buffer_.erase(0, pos);
        }
        
    } catch (const std::exception& e) {
        HandleError(std::string("Parser exception: ") + e.what());
        result.success = false;
        result.error_message = e.what();
    }
    
    return result;
}
// ... existing code ...
ParseResult HttpParser::ParseStartLine(const char* data, size_t len, size_t& pos) {
    // 查找请求行结束标记（CRLF）
    size_t line_end = FindCRLF(data, len, pos);
    if (line_end == std::string::npos) {
        ParseResult result;
        result.success = true;
        result.state = state_;
        result.bytes_parsed = total_bytes_parsed_;
        result.error_message = "";
        return result; // 需要更多数据
    }
    
    // 提取整行数据
    std::string line(data + pos, line_end - pos);
    pos = line_end + 2; // 跳过CRLF
    
    // 解析请求行：方法、路径、版本
    std::istringstream iss(line);
    std::string method, path, version;
    
    if (!(iss >> method >> path >> version)) {
        HandleError("Malformed request line");
        ParseResult result;
        result.success = false;
        result.state = state_;
        result.bytes_parsed = total_bytes_parsed_;
        result.error_message = "Invalid request line format";
        return result;
    }
    
    // 验证HTTP方法和版本
    if (!ValidateHttpMethod(method)) {
        HandleError("Unsupported HTTP method: " + method);
        ParseResult result;
        result.success = false;
        result.state = state_;
        result.bytes_parsed = total_bytes_parsed_;
        result.error_message = "Unsupported HTTP method";
        return result;
    }
    
    if (!ValidateHttpVersion(version)) {
        HandleError("Unsupported HTTP version: " + version);
        ParseResult result;
        result.success = false;
        result.state = state_;
        result.bytes_parsed = total_bytes_parsed_;
        result.error_message = "Unsupported HTTP version";
        return result;
    }
    
    // 设置请求对象
    request_->SetMethod(StringToMethod(method));
    request_->SetPath(path);
    request_->SetVersion(StringToVersion(version));
    
    // 转换到头部解析状态
    TransitionTo(ParseState::HEADERS);
    
    ParseResult result;
    result.success = true;
    result.state = state_;
    result.bytes_parsed = total_bytes_parsed_;
    result.error_message = "";
    return result;
}

ParseResult HttpParser::ParseHeaders(const char* data, size_t len, size_t& pos) {
    ParseResult result;
    result.success = true;
    result.state = state_;
    result.bytes_parsed = total_bytes_parsed_;
    result.error_message = "";
    
    while (pos < len) {
        // 查找行结束标记
        size_t line_end = FindCRLF(data, len, pos);
        if (line_end == std::string::npos) {
            return result; // 需要更多数据
        }
        
        // 空行表示头部结束
        if (line_end == pos) {
            pos = line_end + 2; // 跳过空行的CRLF
            
            // 检查是否需要解析正文
            std::string content_length = request_->GetHeader("Content-Length");
            std::string transfer_encoding = request_->GetHeader("Transfer-Encoding");
            
            if (!content_length.empty()) {
                try {
                    content_length_ = std::stoul(content_length);
                    TransitionTo(ParseState::BODY);
                } catch (const std::exception&) {
                    HandleError("Invalid Content-Length header");
                    result.success = false;
                    result.error_message = "Invalid Content-Length";
                    return result;
                }
            } else if (transfer_encoding == "chunked") {
                chunked_encoding_ = true;
                TransitionTo(ParseState::CHUNKED_BODY);
            } else {
                // 无正文，解析完成
                TransitionTo(ParseState::COMPLETE);
            }
            break;
        }
        
        // 解析单个头部字段
        std::string line(data + pos, line_end - pos);
        pos = line_end + 2;
        
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            HandleError("Malformed header: " + line);
            result.success = false;
            result.state = state_;
            result.bytes_parsed = total_bytes_parsed_;
            result.error_message = "Invalid header format";
            return result;
        }
        
        std::string name = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // 去除首尾空白字符
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        request_->AddHeader(name, value);
    }
    
    return result;
}

ParseResult HttpParser::ParseBody(const char* data, size_t len, size_t& pos) {
    ParseResult result;
    result.success = true;
    result.state = state_;
    result.bytes_parsed = total_bytes_parsed_;
    result.error_message = "";
    
    size_t bytes_remaining = len - pos;
    size_t bytes_needed = content_length_ - request_->GetBody().size();
    
    if (bytes_remaining >= bytes_needed) {
        // 有足够数据完成正文解析
        request_->AppendBody(data + pos, bytes_needed);
        pos += bytes_needed;
        TransitionTo(ParseState::COMPLETE);
    } else {
        // 需要更多数据
        request_->AppendBody(data + pos, bytes_remaining);
        pos += bytes_remaining;
    }
    
    return result;
}

ParseResult HttpParser::ParseChunkedBody(const char* data, size_t len, size_t& pos) {
    ParseResult result;
    result.success = true;
    result.state = state_;
    result.bytes_parsed = total_bytes_parsed_;
    result.error_message = "";
    
    while (pos < len) {
        if (!chunk_size_parsed_) {
            // 解析分块大小行
            size_t line_end = FindCRLF(data, len, pos);
            if (line_end == std::string::npos) {
                return result; // 需要更多数据
            }
            
            std::string chunk_size_line(data + pos, line_end - pos);
            pos = line_end + 2;
            
            // 解析分块大小（十六进制）
            try {
                current_chunk_size_ = std::stoul(chunk_size_line, nullptr, 16);
                chunk_size_parsed_ = true;
                
                // 分块大小为0表示正文结束
                if (current_chunk_size_ == 0) {
                    TransitionTo(ParseState::COMPLETE);
                    break;
                }
            } catch (const std::exception&) {
                HandleError("Invalid chunk size: " + chunk_size_line);
                result.success = false;
                result.state = state_;
                result.bytes_parsed = total_bytes_parsed_;
                result.error_message = "Invalid chunk size";
                return result;
            }
        } else {
            // 解析分块数据
            size_t bytes_available = len - pos;
            size_t bytes_to_read = std::min(current_chunk_size_, bytes_available);
            
            request_->AppendBody(data + pos, bytes_to_read);
            pos += bytes_to_read;
            current_chunk_size_ -= bytes_to_read;
            
            // 当前分块读取完成
            if (current_chunk_size_ == 0) {
                chunk_size_parsed_ = false;
                
                // 跳过分块结束的CRLF
                if (pos + 2 <= len && data[pos] == '\r' && data[pos + 1] == '\n') {
                    pos += 2;
                }
            }
        }
    }
    
    return result;
}

// 添加缺失的辅助函数实现
size_t HttpParser::FindCRLF(const char* data, size_t len, size_t start_pos) const {
    for (size_t i = start_pos; i + 1 < len; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::string::npos;
}

bool HttpParser::ValidateHttpMethod(const std::string& method) const {
    // 检查是否为有效的HTTP方法
    static const std::unordered_set<std::string> valid_methods = {
        "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH", "TRACE", "CONNECT"
    };
    
    return valid_methods.find(method) != valid_methods.end();
}

bool HttpParser::ValidateHttpVersion(const std::string& version) const {
    // 检查是否为有效的HTTP版本
    static const std::unordered_set<std::string> valid_versions = {
        "HTTP/1.0", "HTTP/1.1", "HTTP/2.0"
    };
    
    return valid_versions.find(version) != valid_versions.end();
}

void HttpParser::TransitionTo(ParseState new_state) {
    state_ = new_state;
}

void HttpParser::HandleError(const std::string& message) {
    state_ = ParseState::ERROR;
    // 可以在这里添加错误日志记录
    std::cerr << "HTTP Parser Error: " << message << std::endl;
}

std::unique_ptr<HttpRequest> HttpParser::GetRequest() {
    return std::move(request_);
}

void HttpParser::Reset() {
    state_ = ParseState::START_LINE;
    content_length_ = 0;
    chunked_encoding_ = false;
    buffer_.clear();
    total_bytes_parsed_ = 0;
    current_chunk_size_ = 0;
    chunk_size_parsed_ = false;
    request_ = std::make_unique<HttpRequest>();
}

bool HttpParser::IsParsing() const {
    return state_ != ParseState::COMPLETE && state_ != ParseState::ERROR;
}

ParseState HttpParser::GetCurrentState() const {
    return state_;
}

}