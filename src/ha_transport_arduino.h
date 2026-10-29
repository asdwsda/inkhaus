#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "http_transport.h"
#include "keep_alive.h"

// HTTPClient-based transport. http:// uses WiFiClient, https:// uses WiFiClientSecure with the CA
// bundle (see .cpp for the embedding requirement). One connection is kept open and reused for all
// requests (task 036, policy in lib/ha_client/keep_alive); close() drops it before sleep.
class ArduinoHttpTransport : public HttpTransport {
 public:
  explicit ArduinoHttpTransport(uint32_t connect_timeout_ms = 4000, uint32_t read_timeout_ms = 8000)
      : raw_(connect_timeout_ms, read_timeout_ms), keep_(raw_) {}

  HttpResponse request(const std::string& method, const std::string& url,
                       const HttpHeaders& headers, const std::string& body) override {
    return keep_.request(method, url, headers, body);
  }
  // Streaming (task 051): own request on the shared sockets, no 64 KB cap. Drops the kept
  // connection first; the streaming connection is dropped after the call (one-shot).
  StreamResult stream(const std::string& method, const std::string& url, const HttpHeaders& headers,
                      const std::string& body, const StreamLimits& lim, const ChunkFn& sink) override {
    keep_.close();
    return raw_.stream(method, url, headers, body, lim, sink);
  }
  void close() override { keep_.close(); }

 private:
  // The socket holder: the clients and the HTTPClient live as long as the transport so that the
  // connection survives between requests.
  class Raw : public RawHttp {
   public:
    Raw(uint32_t connect_timeout_ms, uint32_t read_timeout_ms)
        : connect_timeout_ms_(connect_timeout_ms), read_timeout_ms_(read_timeout_ms) {}
    HttpResponse send(const std::string& method, const std::string& url, const HttpHeaders& headers,
                      const std::string& body) override;
    void drop() override;
    StreamResult stream(const std::string& method, const std::string& url, const HttpHeaders& headers,
                        const std::string& body, const StreamLimits& lim, const ChunkFn& sink);

   private:
    uint32_t connect_timeout_ms_, read_timeout_ms_;
    bool ca_set_ = false;
    WiFiClient plain_;
    WiFiClientSecure secure_;
    HTTPClient http_;  // after the clients: its destructor stops the one it points to
  };
  Raw raw_;
  KeepAliveTransport keep_;
};
