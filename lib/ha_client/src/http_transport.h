#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct HttpResponse {
  int status = 0;     // HTTP status; 0 = no response (connect failure / timeout)
  std::string body;
  std::string error;  // transport error text when status == 0
  bool too_large = false;  // body exceeded the size cap: not a connectivity failure
  bool reused = false;     // sent over an already open connection (keep-alive)
  bool stale = false;      // status == 0 and reused: the old connection failed before any response
  uint32_t ms = 0;         // duration of the request
};

class HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  virtual HttpResponse request(const std::string& method, const std::string& url,
                               const HttpHeaders& headers, const std::string& body) = 0;
  // Closes a kept-alive connection (before sleep). No-op for transports without one.
  virtual void close() {}
};
