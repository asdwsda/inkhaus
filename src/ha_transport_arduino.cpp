#include "ha_transport_arduino.h"

#include "body_limit.h"
#include "stream_pump.h"

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

// StreamSource over a connected WiFiClient (task 051 fix).
class ClientSource : public StreamSource {
 public:
  explicit ClientSource(WiFiClient* c) : c_(c) {}
  int read(uint8_t* buf, size_t cap) override {
    int avail = c_->available();
    if (avail > 0) {
      int n = c_->read(buf, cap < (size_t)avail ? cap : (size_t)avail);
      if (n > 0) return n;
    }
    return c_->connected() || c_->available() > 0 ? 0 : -1;
  }
  uint32_t now() override { return millis(); }
  void idle() override { delay(5); }

 private:
  WiFiClient* c_;
};
}  // namespace

HttpResponse ArduinoHttpTransport::Raw::send(const std::string& method, const std::string& url,
                                             const HttpHeaders& headers, const std::string& body) {
  const uint32_t start = millis();
  HttpResponse out;
  const bool https = url.rfind("https://", 0) == 0;
  WiFiClient* client = &plain_;
  if (https) {
    if (!ca_bundle_start) {
      out.error = "https: CA bundle not embedded";
      return out;
    }
    if (!ca_set_) {
      secure_.setCACertBundle(ca_bundle_start);
      ca_set_ = true;
    }
    client = &secure_;
  }
  // An open socket is reused by HTTPClient (HTTPClient.cpp:1128 connect() "already connected,
  // reusing"; "Connection: keep-alive" at :1209 because setReuse(true)); WiFiClientSecure::connected()
  // probes the socket, so a connection the server closed meanwhile reads as not connected here.
  out.reused = client->connected();

  http_.setConnectTimeout(connect_timeout_ms_);
  http_.setTimeout(read_timeout_ms_);
  http_.setReuse(true);
  if (!http_.begin(*client, url.c_str())) {
    out.reused = false;
    out.error = "begin failed";
    return out;
  }
  for (const auto& h : headers) http_.addHeader(h.first.c_str(), h.second.c_str());

  bool keep = false;  // false: the connection state is unclear, drop it
  int code = http_.sendRequest(method.c_str(), (uint8_t*)body.data(), body.size());
  if (code > 0) {
    if (content_length_too_large(http_.getSize())) {  // -1 when unknown/chunked
      out.too_large = true;
      out.error = "response too large";
    } else {
      CappedStream capped(start);
      // writeToStream decodes chunked and identity bodies; with no body
      // (204 / Content-Length 0) it returns 0 without touching the stream.
      int ret = http_.writeToStream(&capped);
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
        keep = true;
      }
    }
  } else {
    out.error = HTTPClient::errorToString(code).c_str();
    // Failed while sending or before any response byte on a reused socket: the server most
    // likely closed it while idle (HTTPC_ERROR_CONNECTION_REFUSED..CONNECTION_LOST = -1..-5).
    out.stale = out.reused && code <= -1 && code >= -5;
  }
  // end() keeps the socket open when keep-alive is on and the server did not say "close"
  // (HTTPClient.cpp:384 disconnect(): "tcp keep open for reuse").
  http_.end();
  if (!keep) drop();
  out.ms = millis() - start;
  return out;
}

void ArduinoHttpTransport::Raw::drop() {
  plain_.stop();
  secure_.stop();
}

// UNVERIFIED: the body is read straight from getStreamPtr() in 512 byte slices (stream_pump), so the
// 15 s deadline and the cancel flag are checked even when the server stalls; Transfer-Encoding is
// read via collectHeaders(); HTTPClient has consumed exactly the headers when sendRequest returns.
// UNVERIFIED: no heap spike (no body accumulation).
StreamResult ArduinoHttpTransport::Raw::stream(const std::string& method, const std::string& url,
                                               const HttpHeaders& headers, const std::string& body,
                                               const StreamLimits& lim, const ChunkFn& sink) {
  const uint32_t start = millis();
  StreamResult out;
  const bool https = url.rfind("https://", 0) == 0;
  WiFiClient* client = &plain_;
  if (https) {
    if (!ca_bundle_start) {
      out.error = "https: CA bundle not embedded";
      return out;
    }
    if (!ca_set_) {
      secure_.setCACertBundle(ca_bundle_start);
      ca_set_ = true;
    }
    client = &secure_;
  }
  http_.setConnectTimeout(connect_timeout_ms_);
  http_.setTimeout(read_timeout_ms_);
  http_.setReuse(false);
  if (!http_.begin(*client, url.c_str())) {
    out.error = "begin failed";
    return out;
  }
  for (const auto& h : headers) http_.addHeader(h.first.c_str(), h.second.c_str());
  static const char* kCollect[] = {"Transfer-Encoding"};
  http_.collectHeaders(kCollect, 1);
  int code = http_.sendRequest(method.c_str(), (uint8_t*)body.data(), body.size());
  if (code > 0) {
    out.status = code;
    if (code >= 200 && code < 300) {
      const bool chunked = http_.header("Transfer-Encoding").indexOf("chunked") >= 0;
      ClientSource src(http_.getStreamPtr());
      pump_body(src, chunked ? BodyKind::Chunked : BodyKind::Identity, chunked ? -1 : http_.getSize(),
                lim, start, sink, out);
    }
  } else {
    out.error = HTTPClient::errorToString(code).c_str();
  }
  http_.end();
  http_.setReuse(true);
  out.ms = millis() - start;
  return out;
}
