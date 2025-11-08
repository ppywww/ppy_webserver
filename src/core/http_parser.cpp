#include "http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

namespace ppsever {

HttpParser::HttpParser() 
    : state_(ParseState::START_LINE),
      content_length_(0),
      chunked_encoding_(false),
      total_bytes_parsed_(0),
      current_chunk_size_(0),
      chunk_size_parsed_(false) {
    
    request_ = std::make_unique<HttpRequest>();
}

ParseResult HttpParser::Parse(const char* data, size_t len) {
    if (len == 0 || data == nullptr) {
        return ParseResult{false, state_, total_bytes_parsed_, "Invalid input data"};
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

ParseResult HttpParser::ParseStartLine(const char* data, size_t len, size_t& pos) {
    // 查找请求行结束标记（CRLF）
    size_t line_end = FindCRLF(data, len, pos);
    if (line_end == std::string::npos) {
        return ParseResult{true, state_, total_bytes_parsed_, ""}; // 需要更多数据
    }
    
    // 提取整行数据
    std::string line(data + pos, line_end - pos);
    pos = line_end + 2; // 跳过CRLF
    
    // 解析请求行：方法、路径、版本
    std::istringstream iss(line);
    std::string method, path, version;
    
    if (!(iss >> method >> path >> version)) {
        HandleError("Malformed request line");
        return ParseResult{false, state_, total_bytes_parsed_, "Invalid request line format"};
    }
    
    // 验证HTTP方法和版本
    if (!ValidateHttpMethod(method)) {
        HandleError("Unsupported HTTP method: " + method);
        return ParseResult{false, state_, total_bytes_parsed_, "Unsupported HTTP method"};
    }
    
    if (!ValidateHttpVersion(version)) {
        HandleError("Unsupported HTTP version: " + version);
        return ParseResult{false, state_, total_bytes_parsed_, "Unsupported HTTP version"};
    }
    
    // 设置请求对象
    request_->SetMethod(method);
    request_->SetPath(path);
    request_->SetVersion(version);
    
    // 转换到头部解析状态
    TransitionTo(ParseState::HEADERS);
    return ParseResult{true, state_, total_bytes_parsed_, ""};
}

ParseResult HttpParser::ParseHeaders(const char* data, size_t len, size_t& pos) {
    while (pos < len) {
        // 查找行结束标记
        size_t line_end = FindCRLF(data, len, pos);
        if (line_end == std::string::npos) {
            return ParseResult{true, state_, total_bytes_parsed_, ""}; // 需要更多数据
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
                    return ParseResult{false, state_, total_bytes_parsed_, "Invalid Content-Length"};
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
            return ParseResult{false, state_, total_bytes_parsed_, "Invalid header format"};
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
    
    return ParseResult{true, state_, total_bytes_parsed_, ""};
}

ParseResult HttpParser::ParseBody(const char* data, size_t len, size_t& pos) {
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
    
    return ParseResult{true, state_, total_bytes_parsed_, ""};
}

ParseResult HttpParser::ParseChunkedBody(const char* data, size_t len, size_t& pos) {
    while (pos < len) {
        if (!chunk_size_parsed_) {
            // 解析分块大小行
            size_t line_end = FindCRLF(data, len, pos);
            if (line_end == std::string::npos) {
                return ParseResult{true, state_, total_bytes_parsed_, ""}; // 需要更多数据
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
                return ParseResult{false, state_, total_bytes_parsed_, "Invalid chunk size"};
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
    
    return ParseResult{true, state_, total_bytes_parsed_, ""};
}

std::unique_ptr<HttpRequest> HttpParser::GetRequest() {
    if (state_ == ParseState::COMPLETE) {
        return std::move(request_);
    }
    return nullptr;
}

void HttpParser::Reset() {
    state_ = ParseState::START_LINE;
    request_ = std::make_unique<HttpRequest>();
    content_length_ = 0;
    chunked_encoding_ = false;
    buffer_.clear();
    total_bytes_parsed_ = 0;
    current_chunk_size_ = 0;
    chunk_size_parsed_ = false;
}

// 辅助方法实现
size_t HttpParser::FindCRLF(const char* data, size_t len, size_t start_pos) const {
    for (size_t i = start_pos; i + 1 < len; ++i) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return i;
        }
    }
    return std::string::npos;
}

bool HttpParser::ValidateHttpVersion(const std::string& version) const {
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}

bool HttpParser::ValidateHttpMethod(const std::string& method) const {
    static const std::unordered_map<std::string, bool> valid_methods = {
        {"GET", true}, {"POST", true}, {"PUT", true}, {"DELETE", true},
        {"HEAD", true}, {"OPTIONS", true}, {"PATCH", true}, {"TRACE", true}
    };
    return valid_methods.find(method) != valid_methods.end();
}

void HttpParser::TransitionTo(ParseState new_state) {
    state_ = new_state;
}

void HttpParser::HandleError(const std::string& message) {
    state_ = ParseState::ERROR;
    std::cerr << "HTTP Parser Error: " << message << std::endl;
}

bool HttpParser::IsParsing() const {
    return state_ != ParseState::COMPLETE && state_ != ParseState::ERROR;
}

ParseState HttpParser::GetCurrentState() const {
    return state_;
}

} // namespace ppsever