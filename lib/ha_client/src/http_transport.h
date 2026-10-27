#pragma once
#include <string>
#include <utility>
#include <vector>

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpResponse {
  int status = 0;     // HTTP status; 0 = no response (connect failure / timeout)
  std::string body;
  std::string error;  // transport error text when status == 0
  bool too_large = false;  // body exceeded the size cap: not a connectivity failure
};

class HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  virtual HttpResponse request(const std::string& method, const std::string& url,
                               const HttpHeaders& headers, const std::string& body) = 0;
};
