#pragma once
#include "http_transport.h"

// HTTPClient-based transport. http:// uses WiFiClient, https:// uses
// WiFiClientSecure with the CA bundle (see .cpp for the embedding requirement).
class ArduinoHttpTransport : public HttpTransport {
 public:
  explicit ArduinoHttpTransport(uint32_t connect_timeout_ms = 4000,
                                uint32_t read_timeout_ms = 8000)
      : connect_timeout_ms_(connect_timeout_ms), read_timeout_ms_(read_timeout_ms) {}

  HttpResponse request(const std::string& method, const std::string& url,
                       const HttpHeaders& headers, const std::string& body) override;

 private:
  uint32_t connect_timeout_ms_;
  uint32_t read_timeout_ms_;
};
