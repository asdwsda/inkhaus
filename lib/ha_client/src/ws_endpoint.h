#pragma once
// Where to open the WebSocket for a HA base URL (task 055). http:// -> plain ws (port 80), https:// -> TLS
// (port 443). Path and query of the base URL are ignored (the endpoint is always /api/websocket).
#include <cstdint>
#include <string>

namespace ws {

struct Endpoint {
  bool valid = false;
  bool tls = false;
  std::string host;     // without port
  uint16_t port = 0;
  std::string hostHeader;  // host[:port], the port only when it is not the scheme's default
};

Endpoint parse_endpoint(const std::string& baseUrl);

}  // namespace ws
