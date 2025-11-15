#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "connection.hpp"
#include "connection_manager.hpp"
#include "event_loop.hpp"

namespace ppserver {

class HttpResponse {
public:
    enum class HttpStatusCode {
        OK = 200,
        NOT_FOUND = 404,
        INTERNAL_SERVER_ERROR = 500
    };

    HttpResponse();
    ~HttpResponse();

    void SetStatusCode(HttpStatusCode code);
    void SetHeader(const std::string& key, const std::string& value);
    void SetBody(const std::string& body);

    HttpStatusCode GetStatusCode() const;
    const std::unordered_map<std::string, std::string>& GetHeaders() const;
    const std::string& GetBody() const;

private:
    HttpStatusCode status_code_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace ppsever