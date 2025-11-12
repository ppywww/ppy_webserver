#include "http_response.hpp"

namespace ppsever {

HttpResponse::HttpResponse() 
    : status_code_(HttpStatusCode::OK) {
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::SetStatusCode(HttpStatusCode code) {
    status_code_ = code;
}

void HttpResponse::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::SetBody(const std::string& body) {
    body_ = body;
}

HttpResponse::HttpStatusCode HttpResponse::GetStatusCode() const {
    return status_code_;
}

const std::unordered_map<std::string, std::string>& HttpResponse::GetHeaders() const {
    return headers_;
}

const std::string& HttpResponse::GetBody() const {
    return body_;
}

} // namespace ppsever