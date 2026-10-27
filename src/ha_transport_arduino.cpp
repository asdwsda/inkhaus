#include "ha_transport_arduino.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "body_limit.h"

// Arduino-ESP32 2.0.x does not ship a ready CA bundle: the app must embed a
// binary x509 bundle (generated from the Mozilla roots, e.g. with ESP-IDF's
// gen_crt_bundle.py) via platformio.ini:
//   board_build.embed_files = certs/x509_crt_bundle
// which defines the symbol below. It is declared weak so the firmware links
// without it; https requests then fail with a clear error instead of falling
// back to an insecure connection.
extern const uint8_t ca_bundle_start[] asm("_binary_certs_x509_crt_bundle_start") __attribute__((weak));

namespace {
// Stream adapter feeding HTTPClient::writeToStream into a BodySink. A refused
// write returns 0 (short write); HTTPClient retries once, gets 0 again and
// aborts with HTTPC_ERROR_STREAM_WRITE, which request() maps via the sink flags.
class CappedStream : public Stream {
 public:
  explicit CappedStream(uint32_t start_ms) : start_(start_ms) {}
  size_t write(const uint8_t* p, size_t n) override {
    return sink.accept(p, n, millis() - start_);
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  BodySink sink;

 private:
  uint32_t start_;
};
}  // namespace

HttpResponse ArduinoHttpTransport::request(const std::string& method, const std::string& url,
                                           const HttpHeaders& headers, const std::string& body) {
  const uint32_t start = millis();
  HttpResponse out;
  const bool https = url.rfind("https://", 0) == 0;

  WiFiClient plain;
  WiFiClientSecure secure;
  if (https) {
    if (!ca_bundle_start) {
      out.error = "https: CA bundle not embedded";
      return out;
    }
    secure.setCACertBundle(ca_bundle_start);
  }

  HTTPClient http;
  http.setConnectTimeout(connect_timeout_ms_);
  http.setTimeout(read_timeout_ms_);
  http.setReuse(false);
  bool begun = https ? http.begin(secure, url.c_str()) : http.begin(plain, url.c_str());
  if (!begun) {
    out.error = "begin failed";
    return out;
  }
  for (const auto& h : headers) http.addHeader(h.first.c_str(), h.second.c_str());

  int code = http.sendRequest(method.c_str(), (uint8_t*)body.data(), body.size());
  if (code > 0) {
    if (content_length_too_large(http.getSize())) {  // -1 when unknown/chunked
      out.too_large = true;
      out.error = "response too large";
    } else {
      CappedStream capped(start);
      // writeToStream decodes chunked and identity bodies; with no body
      // (204 / Content-Length 0) it returns 0 without touching the stream.
      int ret = http.writeToStream(&capped);
      if (capped.sink.too_large) {
        out.too_large = true;
        out.error = "response too large";
      } else if (capped.sink.deadline_exceeded) {
        out.error = "request deadline exceeded";
      } else if (ret < 0) {
        out.error = HTTPClient::errorToString(ret).c_str();
      } else {
        out.status = code;
        out.body = std::move(capped.sink.data);
      }
    }
  } else {
    out.error = HTTPClient::errorToString(code).c_str();
  }
  http.end();
  return out;
}
