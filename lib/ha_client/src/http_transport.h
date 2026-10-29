#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "stream.h"

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
  // Streaming request (task 051): the body goes to `sink` chunk by chunk, no 64 KB cap; limits
  // from `lim`. Default: buffered request() replayed to the sink (fakes, transports without
  // streaming); the cap of request() then still applies.
  virtual StreamResult stream(const std::string& method, const std::string& url,
                              const HttpHeaders& headers, const std::string& body,
                              const StreamLimits& lim, const ChunkFn& sink) {
    HttpResponse r = request(method, url, headers, body);
    StreamResult o;
    o.status = r.status;
    o.error = r.error;
    o.too_large = r.too_large;
    o.reused = r.reused;
    o.ms = r.ms;
    if (r.status >= 200 && r.status < 300) {
      StreamGate gate(lim, sink);
      for (size_t i = 0; i < r.body.size();) {
        size_t n = r.body.size() - i < 1024 ? r.body.size() - i : 1024;
        if (gate.accept(r.body.data() + i, n, 0, o) == 0) break;
        i += n;
      }
    }
    return o;
  }
  // Closes a kept-alive connection (before sleep). No-op for transports without one.
  virtual void close() {}
};
