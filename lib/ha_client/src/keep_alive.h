#pragma once
// Keep-alive policy around a raw HTTP sender: one connection is kept open and reused for all
// requests of a wake. Pure logic (no Arduino); the device supplies a RawHttp that holds the socket.
//  - a request to another scheme://host:port drops the open connection first;
//  - a reused connection that fails before any response (server closed it while idle) is dropped
//    and the request is repeated once on a fresh connection;
//  - close() drops the connection (before sleep).
#include "http_transport.h"

class RawHttp {
 public:
  virtual ~RawHttp() = default;
  // Sends over the open connection if there is one (resp.reused = true), else connects first.
  // After a failure on a reused connection it sets resp.stale.
  virtual HttpResponse send(const std::string& method, const std::string& url,
                            const HttpHeaders& headers, const std::string& body) = 0;
  virtual void drop() = 0;  // closes the connection, if any
};

// "https://host:8123" for "https://host:8123/api/x" (the part before the path).
std::string url_origin(const std::string& url);

class KeepAliveTransport : public HttpTransport {
 public:
  explicit KeepAliveTransport(RawHttp& raw) : raw_(raw) {}
  HttpResponse request(const std::string& method, const std::string& url,
                       const HttpHeaders& headers, const std::string& body) override;
  void close() override;

 private:
  RawHttp& raw_;
  std::string origin_;  // origin of the open connection, empty = none
};
