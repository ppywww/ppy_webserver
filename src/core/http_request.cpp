// http_request.cpp
#include "http_request.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>


namespace ppserver {

// 静态常量初始化
const std::string HttpRequest::EMPTY_STRING = "";

const std::unordered_map<HttpRequest::Method, std::string> 
HttpRequest::METHOD_STRINGS = {
    {Method::GET, "GET"},
    {Method::POST, "POST"},
    {Method::PUT, "PUT"},
    {Method::DELETE, "DELETE"},
    {Method::HEAD, "HEAD"},
    {Method::OPTIONS, "OPTIONS"},
    {Method::PATCH, "PATCH"},
    {Method::TRACE, "TRACE"},
    {Method::CONNECT, "CONNECT"},
    {Method::UNKNOWN, "UNKNOWN"}
};

const std::unordered_map<HttpRequest::Version, std::string> 
HttpRequest::VERSION_STRINGS = {
    {Version::HTTP_1_0, "HTTP/1.0"},
    {Version::HTTP_1_1, "HTTP/1.1"},
    {Version::HTTP_2_0, "HTTP/2.0"},
    {Version::UNKNOWN, "UNKNOWN"}
};

// 构造函数
HttpRequest::HttpRequest() 
    : method_(Method::UNKNOWN),
      version_(Version::UNKNOWN),
      query_parsed_(false),
      receive_time_(0),
      request_id_(0) {
}

// 请求行设置接口实现
void HttpRequest::SetMethod(Method method) {
    method_ = method;
}

void HttpRequest::SetPath(const std::string& path) {
    path_ = path;
    query_parsed_ = false; // 路径改变时需要重新解析查询参数
}

void HttpRequest::SetVersion(Version version) {
    version_ = version;
}

void HttpRequest::SetQueryString(const std::string& query) {
    query_string_ = query;
    query_parsed_ = false; // 设置新的查询字符串需要重新解析
}

// 头部字段管理实现
void HttpRequest::AddHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
    headers_lowercase_[ToLower(name)] = value;
}

void HttpRequest::SetHeaders(std::unordered_map<std::string, std::string> headers) {
    headers_ = std::move(headers);
    headers_lowercase_.clear();
    for (const auto& [name, value] : headers_) {
        headers_lowercase_[ToLower(name)] = value;
    }
}

bool HttpRequest::RemoveHeader(const std::string& name) {
    auto it = headers_.find(name);
    if (it != headers_.end()) {
        headers_lowercase_.erase(ToLower(name));
        headers_.erase(it);
        return true;
    }
    return false;
}

void HttpRequest::ClearHeaders() {
    headers_.clear();
    headers_lowercase_.clear();
}

// 消息体管理实现
void HttpRequest::SetBody(const std::string& body) {
    body_ = body;
}

void HttpRequest::AppendBody(const char* data, size_t length) {
    body_.append(data, length);
}

void HttpRequest::ClearBody() {
    body_.clear();
}

// 查询参数管理实现
void HttpRequest::ParseQueryParameters() {
    if (query_parsed_ || query_string_.empty()) {
        return;
    }
    
    query_params_.clear();
    ParseQueryString(query_string_);
    query_parsed_ = true;
}

void HttpRequest::AddQueryParameter(const std::string& key, const std::string& value) {
    query_params_[key] = value;
}

// 元数据设置实现
void HttpRequest::SetRemoteAddress(const std::string& address) {
    remote_address_ = address;
}

void HttpRequest::SetReceiveTime(std::time_t time) {
    receive_time_ = time;
}

void HttpRequest::SetRequestId(uint64_t id) {
    request_id_ = id;
}

// 常量访问接口实现
HttpRequest::Method HttpRequest::GetMethod() const {
    return method_;
}

const std::string& HttpRequest::GetMethodString() const {
    auto it = METHOD_STRINGS.find(method_);
    return it != METHOD_STRINGS.end() ? it->second : EMPTY_STRING;
}

const std::string& HttpRequest::GetPath() const {
    return path_;
}

HttpRequest::Version HttpRequest::GetVersion() const {
    return version_;
}

const std::string& HttpRequest::GetVersionString() const {
    auto it = VERSION_STRINGS.find(version_);
    return it != VERSION_STRINGS.end() ? it->second : EMPTY_STRING;
}

const std::string& HttpRequest::GetQueryString() const {
    return query_string_;
}

// 头部字段访问实现
const std::string& HttpRequest::GetHeader(const std::string& name) const {
    auto it = headers_lowercase_.find(ToLower(name));// 使用lowercase
    //
    return it != headers_lowercase_.end() ? it->second : EMPTY_STRING;
}

const std::unordered_map<std::string, std::string>& 
HttpRequest::GetAllHeaders() const {
    return headers_;
}

bool HttpRequest::HasHeader(const std::string& name) const {
    return headers_lowercase_.find(ToLower(name)) != headers_lowercase_.end();
}

std::vector<std::string> HttpRequest::GetHeaderNames() const {
    std::vector<std::string> names;
    for (const auto& [name, value] : headers_) {
        names.push_back(name);
    }
    return names;
}

// 消息体访问实现
const std::string& HttpRequest::GetBody() const {
    return body_;
}

size_t HttpRequest::GetBodySize() const {
    return body_.size();
}

bool HttpRequest::IsBodyEmpty() const {
    return body_.empty();
}

// 查询参数访问实现
const std::string& HttpRequest::GetQueryParameter(const std::string& key) const {
    auto it = query_params_.find(key);
    return it != query_params_.end() ? it->second : EMPTY_STRING;
}

const std::unordered_map<std::string, std::string>& 
HttpRequest::GetAllQueryParameters() const {
    return query_params_;
}

bool HttpRequest::HasQueryParameter(const std::string& key) const {
    return query_params_.find(key) != query_params_.end();
}

std::vector<std::string> HttpRequest::GetQueryParameterNames() const {
    std::vector<std::string> names;
    for (const auto& [key, value] : query_params_) {
        names.push_back(key);
    }
    return names;
}

// 元数据访问实现
const std::string& HttpRequest::GetRemoteAddress() const {
    return remote_address_;
}

std::time_t HttpRequest::GetReceiveTime() const {
    return receive_time_;
}

uint64_t HttpRequest::GetRequestId() const {
    return request_id_;
}

// 内容类型辅助方法实现
std::string HttpRequest::GetContentType() const {
    std::string contentType = GetHeader("Content-Type");
    size_t pos = contentType.find(';');
    if (pos != std::string::npos) {
        return contentType.substr(0, pos);
    }
    return contentType;
}

std::string HttpRequest::GetCharset() const {
    std::string contentType = GetHeader("Content-Type");
    size_t pos = contentType.find("charset=");
    if (pos != std::string::npos) {
        pos += 8; // "charset="长度
        size_t end = contentType.find(';', pos);
        if (end == std::string::npos) {
            end = contentType.length();
        }
        return contentType.substr(pos, end - pos);
    }
    return "utf-8"; // 默认字符集
}

size_t HttpRequest::GetContentLength() const {
    auto contentLength = GetHeader("Content-Length");
    if (!contentLength.empty()) {
        try {
            return std::stoul(contentLength);
        } catch (const std::exception&) {
            return 0;
        }
    }
    return body_.size(); // 回退到实际body大小
}

bool HttpRequest::IsKeepAlive() const {
    if (version_ == Version::HTTP_1_1) {
        std::string connection = GetHeader("Connection");
        return connection != "close";
    }
    std::string connection = GetHeader("Connection");
    return connection == "keep-alive";
}

bool HttpRequest::IsChunked() const {
    std::string encoding = GetHeader("Transfer-Encoding");
    return encoding.find("chunked") != std::string::npos;
}

// 路径处理辅助方法实现
std::string HttpRequest::GetBasePath() const {
    size_t pos = path_.find_last_of('/');
    if (pos != std::string::npos) {
        return path_.substr(0, pos + 1);
    }
    return "/";
}

std::string HttpRequest::GetExtension() const {
    size_t dot_pos = path_.find_last_of('.');
    size_t slash_pos = path_.find_last_of('/');
    if (dot_pos != std::string::npos && 
        (slash_pos == std::string::npos || dot_pos > slash_pos)) {
        return path_.substr(dot_pos + 1);
    }
    return "";
}

std::string HttpRequest::GetFilename() const {
    size_t slash_pos = path_.find_last_of('/');
    if (slash_pos != std::string::npos) {
        return path_.substr(slash_pos + 1);
    }
    return path_;
}

// 调试和序列化实现
std::string HttpRequest::ToString() const {
    std::stringstream ss;
    ss << GetMethodString() << " " << path_;
    if (!query_string_.empty()) {
        ss << "?" << query_string_;
    }
    ss << " " << GetVersionString() << "\r\n";
    
    for (const auto& [name, value] : headers_) {
        ss << name << ": " << value << "\r\n";
    }
    
    ss << "\r\n";
    if (!body_.empty()) {
        ss << body_;
    }
    
    return ss.str();
}

std::string HttpRequest::HeadersToString() const {
    std::stringstream ss;
    for (const auto& [name, value] : headers_) {
        ss << name << ": " << value << "\n";
    }
    return ss.str();
}

void HttpRequest::PrintDebugInfo() const {
    std::cout << "=== HTTP Request Debug Info ===" << std::endl;
    std::cout << "Method: " << GetMethodString() << std::endl;
    std::cout << "Path: " << path_ << std::endl;
    std::cout << "Version: " << GetVersionString() << std::endl;
    std::cout << "Query: " << query_string_ << std::endl;
    std::cout << "Remote Address: " << remote_address_ << std::endl;
    std::cout << "Headers: " << headers_.size() << " items" << std::endl;
    std::cout << "Body Size: " << body_.size() << " bytes" << std::endl;
    std::cout << "Keep-Alive: " << (IsKeepAlive() ? "Yes" : "No") << std::endl;
}

bool HttpRequest::IsValid() const {
    return method_ != Method::UNKNOWN && 
           version_ != Version::UNKNOWN && 
           !path_.empty();
}

// 私有辅助方法实现
std::string HttpRequest::ToLower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

void HttpRequest::ParseQueryString(const std::string& query) {
    size_t start = 0;
    while (start < query.length()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.length();
        }
        
        size_t equal_pos = query.find('=', start);
        if (equal_pos != std::string::npos && equal_pos < end) {
            std::string key = query.substr(start, equal_pos - start);
            std::string value = query.substr(equal_pos + 1, end - equal_pos - 1);
            
            // URL解码（简化版）
            // 实际项目中应该实现完整的URL解码
            query_params_[key] = value;
        }
        
        start = end + 1;
    }
}

bool HttpRequest::ValidateMethod(const std::string& method) const {
    // 检查方法是否有效
    for (const auto& [m, str] : METHOD_STRINGS) {
        if (str == method) {
            return true;
        }
    }
    return false;
}

bool HttpRequest::ValidateVersion(const std::string& version) const {
    // 检查版本是否有效
    for (const auto& [v, str] : VERSION_STRINGS) {
        if (str == version) {
            return true;
        }
    }
    return false;
}

// 实用函数实现
std::string MethodToString(HttpRequest::Method method) {
    auto it = HttpRequest::METHOD_STRINGS.find(method);
    return it != HttpRequest::METHOD_STRINGS.end() ? it->second : "UNKNOWN";
}

HttpRequest::Method StringToMethod(const std::string& str) {
    for (const auto& [method, method_str] : HttpRequest::METHOD_STRINGS) {
        if (method_str == str) {
            return method;
        }
    }
    return HttpRequest::Method::UNKNOWN;
}

std::string VersionToString(HttpRequest::Version version) {
    auto it = HttpRequest::VERSION_STRINGS.find(version);
    return it != HttpRequest::VERSION_STRINGS.end() ? it->second : "UNKNOWN";
}

HttpRequest::Version StringToVersion(const std::string& str) {
    for (const auto& [version, version_str] : HttpRequest::VERSION_STRINGS) {
        if (version_str == str) {
            return version;
        }
    }
    return HttpRequest::Version::UNKNOWN;
}

} // namespace ppsever