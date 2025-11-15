#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <utility>

namespace ppserver {

class HttpRequest {
public:
    // HTTP方法枚举（支持RFC 7231定义的所有方法）
    enum class Method {
        GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, TRACE, CONNECT, UNKNOWN
    };
    // HTTP版本枚举
    enum class Version {
        HTTP_1_0, HTTP_1_1, HTTP_2_0, UNKNOWN
    };

    //  // HTTP方法字符串映射
    static const std::unordered_map<Method, std::string> METHOD_STRINGS;
    static const std::unordered_map<Version, std::string> VERSION_STRINGS;

    
    
    // 构造函数与析构函数
    HttpRequest();
    ~HttpRequest() = default;

    // 禁止拷贝，允许移动
    HttpRequest(const HttpRequest&) = delete;
    HttpRequest& operator=(const HttpRequest&) = delete;
    HttpRequest(HttpRequest&&) = default;
    HttpRequest& operator=(HttpRequest&&) = default;

    // 请求行设置接口
    void SetMethod(Method method);
    void SetPath(const std::string& path);
    void SetVersion(Version version);
    void SetQueryString(const std::string& query);

    // 头部字段管理
    void AddHeader(const std::string& name, const std::string& value);
    void SetHeaders(std::unordered_map<std::string, std::string> headers);
    bool RemoveHeader(const std::string& name);
    void ClearHeaders();

    // 消息体管理
    void SetBody(const std::string& body);
    void AppendBody(const char* data, size_t length);
    void ClearBody();

    // 查询参数管理（URL参数解析）
    void ParseQueryParameters();
    void AddQueryParameter(const std::string& key, const std::string& value);

    // 元数据设置
    void SetRemoteAddress(const std::string& address);
    void SetReceiveTime(std::time_t time);
    void SetRequestId(uint64_t id);

    // 常量访问接口
    Method GetMethod() const;
    const std::string& GetMethodString() const;
    const std::string& GetPath() const;
    Version GetVersion() const;
    const std::string& GetVersionString() const;
    const std::string& GetQueryString() const;

    // 头部字段访问
    const std::string& GetHeader(const std::string& name) const;
    const std::unordered_map<std::string, std::string>& GetAllHeaders() const;
    bool HasHeader(const std::string& name) const;
    std::vector<std::string> GetHeaderNames() const;

    // 消息体访问
    const std::string& GetBody() const;
    size_t GetBodySize() const;
    bool IsBodyEmpty() const;

    // 查询参数访问
    const std::string& GetQueryParameter(const std::string& key) const;
    const std::unordered_map<std::string, std::string>& GetAllQueryParameters() const;
    bool HasQueryParameter(const std::string& key) const;
    std::vector<std::string> GetQueryParameterNames() const;

    // 元数据访问
    const std::string& GetRemoteAddress() const;
    std::time_t GetReceiveTime() const;
    uint64_t GetRequestId() const;

    // 内容类型辅助方法
    std::string GetContentType() const;
    std::string GetCharset() const;
    size_t GetContentLength() const;
    bool IsKeepAlive() const;
    bool IsChunked() const;

    // 路径处理辅助方法
    std::string GetBasePath() const;
    std::string GetExtension() const;
    std::string GetFilename() const;

    // 调试和序列化
    std::string ToString() const;
    std::string HeadersToString() const;
    void PrintDebugInfo() const;

    // 验证方法
    bool IsValid() const;

private:
    // 请求行数据
    Method method_;
    std::string path_;
    Version version_;
    std::string query_string_;

    // 头部字段（大小写不敏感，但保留原始大小写）
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> headers_lowercase_; // 用于快速查找

    // 消息体
    std::string body_;

    // 查询参数（URL参数）
    std::unordered_map<std::string, std::string> query_params_;
    bool query_parsed_;

    // 元数据
    std::string remote_address_;
    std::time_t receive_time_;
    uint64_t request_id_;

    // 常量字符串
    static const std::string EMPTY_STRING;
 
    // 辅助方法
    std::string ToLower(const std::string& str) const;
    void ParseQueryString(const std::string& query);
    bool ValidateMethod(const std::string& method) const;
    bool ValidateVersion(const std::string& version) const;
};

// 实用函数
std::string MethodToString(HttpRequest::Method method);
HttpRequest::Method StringToMethod(const std::string& str);
std::string VersionToString(HttpRequest::Version version);
HttpRequest::Version StringToVersion(const std::string& str);

} // namespace ppserver