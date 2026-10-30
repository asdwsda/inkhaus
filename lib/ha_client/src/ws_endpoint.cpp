#include "ws_endpoint.h"

namespace ws {

Endpoint parse_endpoint(const std::string& url) {
  Endpoint e;
  size_t p = 0;
  if (url.compare(0, 8, "https://") == 0) { e.tls = true; p = 8; }
  else if (url.compare(0, 7, "http://") == 0) p = 7;
  else return e;
  size_t end = url.find_first_of("/?#", p);
  std::string auth = url.substr(p, end == std::string::npos ? std::string::npos : end - p);
  if (auth.empty() || auth.find('@') != std::string::npos) return e;  // userinfo is not supported
  uint32_t port = e.tls ? 443 : 80;
  bool explicitPort = false;
  size_t colon = auth.rfind(':');
  if (colon != std::string::npos && auth.find(']') == std::string::npos) {
    std::string ps = auth.substr(colon + 1);
    if (ps.empty() || ps.size() > 5) return e;
    uint32_t v = 0;
    for (char c : ps) {
      if (c < '0' || c > '9') return e;
      v = v * 10 + (uint32_t)(c - '0');
    }
    if (v == 0 || v > 65535) return e;
    port = v;
    explicitPort = true;
    auth.resize(colon);
  }
  if (auth.empty()) return e;
  e.host = auth;
  e.port = (uint16_t)port;
  e.hostHeader = auth;
  if (explicitPort && port != (e.tls ? 443u : 80u)) e.hostHeader += ":" + std::to_string(port);
  e.valid = true;
  return e;
}

}  // namespace ws
