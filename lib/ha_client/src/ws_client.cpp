#include "ws_client.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "stats_request.h"
#include "ws_crypto.h"
#include "ws_frame.h"

namespace ws {

std::string upgrade_key(const uint8_t key16[16]) { return base64(key16, 16); }

std::string build_upgrade_request(const std::string& host, const uint8_t key16[16]) {
  return "GET /api/websocket HTTP/1.1\r\nHost: " + host +
         "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + upgrade_key(key16) +
         "\r\nSec-WebSocket-Version: 13\r\n\r\n";
}

namespace {
std::string lower(std::string s) {
  for (auto& c : s) c = (char)tolower((unsigned char)c);
  return s;
}
std::string strip_spaces(const std::string& s) {
  std::string o;
  for (char c : s)
    if (c != ' ' && c != '\n' && c != '\r' && c != '\t') o += c;
  return o;
}
}  // namespace

bool check_upgrade_response(const std::string& head, const std::string& key_b64) {
  size_t eol = head.find("\r\n");
  if (eol == std::string::npos) return false;
  std::string status = head.substr(0, eol);
  if (status.compare(0, 9, "HTTP/1.1 ") != 0 || status.compare(9, 3, "101") != 0) return false;
  std::string want = accept_for_key(key_b64);
  bool accept_ok = false, upgrade_ok = false;
  size_t pos = eol + 2;
  while (pos < head.size()) {
    size_t e = head.find("\r\n", pos);
    if (e == std::string::npos) e = head.size();
    std::string line = head.substr(pos, e - pos);
    pos = e + 2;
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = lower(line.substr(0, colon));
    size_t v = colon + 1;
    while (v < line.size() && line[v] == ' ') v++;
    size_t ve = line.size();
    while (ve > v && line[ve - 1] == ' ') ve--;
    std::string val = line.substr(v, ve - v);
    if (name == "sec-websocket-accept") accept_ok = val == want;
    else if (name == "upgrade") upgrade_ok = lower(val) == "websocket";
  }
  return accept_ok && upgrade_ok;
}

namespace {

enum class Phase { AuthRequired, AuthOk, Result, Done };

class Flow : public FrameHandler {
 public:
  Flow(Socket& s, const Request& r, const ChunkFn& sink)
      : sock_(s), req_(r), gate_(r.limits, sink) {
  }

  FlowResult run();

  bool onData(const uint8_t* p, size_t n, bool first, bool last) override;
  bool onPing(const uint8_t* p, size_t n) override {
    return send(kPong, p, n);
  }
  bool onClose(uint16_t) override {
    if (phase_ != Phase::Done) fail(HaError::Network, "closed early");
    return false;
  }

 private:
  void fail(HaError e, const std::string& msg) {
    if (res_.error != HaError::None) return;
    res_.error = e;
    res_.message = scrub(msg);
  }
  std::string scrub(std::string m) const {
    if (!req_.token.empty()) {
      size_t p;
      while ((p = m.find(req_.token)) != std::string::npos) m.replace(p, req_.token.size(), "***");
    }
    return m;
  }
  void masks(uint8_t m[4]) {
    const uint32_t r = sock_.random32();  // the device socket uses esp_random
    for (int i = 0; i < 4; i++) m[i] = (uint8_t)(r >> (8 * i));
  }
  bool write_all(const std::vector<uint8_t>& f) {
    size_t off = 0;
    while (off < f.size()) {
      int w = sock_.write(f.data() + off, f.size() - off);
      if (w <= 0) { fail(HaError::Network, "write failed"); return false; }
      off += (size_t)w;
    }
    return true;
  }
  bool send(uint8_t op, const uint8_t* p, size_t n) {
    uint8_t m[4];
    masks(m);
    auto f = encode_frame(op, p, n, m);
    if (f.empty() && n > 0) { fail(HaError::Parse, "bad control frame"); return false; }
    return write_all(f);
  }
  bool send_text(const std::string& s) {
    return send(kText, reinterpret_cast<const uint8_t*>(s.data()), s.size());
  }
  void close_now() {
    uint8_t m[4];
    masks(m);
    auto f = encode_close(1000, m);
    size_t off = 0;
    while (off < f.size()) {
      int w = sock_.write(f.data() + off, f.size() - off);
      if (w <= 0) break;
      off += (size_t)w;
    }
    sock_.close();
  }
  bool control_message(const std::string& msg);
  bool result_data(const uint8_t* p, size_t n, bool last);
  bool deliver(const char* p, size_t n);
  bool aborted() const { return res_.error != HaError::None; }

  Socket& sock_;
  const Request& req_;
  StreamGate gate_;
  StreamResult sr_;
  FlowResult res_;
  uint32_t t0_ = 0;
  Phase phase_ = Phase::AuthRequired;
  std::string buf_;       // small messages, or the prefix of the result message
  bool decided_ = false;  // result prefix classified
  bool errMode_ = false;  // buffering a success:false message

  friend FlowResult run_command(Socket&, const Request&, const ChunkFn&);
 public:
  uint32_t elapsed() { return sock_.nowMs() - t0_; }
  void set_t0(uint32_t t) { t0_ = t; }
};

bool Flow::deliver(const char* p, size_t n) {
  if (n == 0) return true;
  size_t took = gate_.accept(p, n, elapsed(), sr_);
  if (took == 0) {
    if (sr_.too_large) fail(HaError::TooLarge, "response too large");
    else if (sr_.deadline) fail(HaError::Timeout, "deadline");
    else fail(HaError::Cancelled, "cancelled");
    return false;
  }
  res_.bytes += n;
  return true;
}

bool Flow::control_message(const std::string& raw) {
  std::string m = strip_spaces(raw);
  auto is = [&](const char* t) { return m.find(std::string("\"type\":\"") + t + "\"") != std::string::npos; };
  if (phase_ == Phase::AuthRequired) {
    if (!is("auth_required")) { fail(HaError::Parse, "unexpected message before auth"); return false; }
    phase_ = Phase::AuthOk;
    return send_text("{\"type\":\"auth\",\"access_token\":" + json_quote(req_.token) + "}");
  }
  // Phase::AuthOk
  if (is("auth_ok")) {
    phase_ = Phase::Result;
    return send_text(req_.command_json);
  }
  if (is("auth_invalid")) { fail(HaError::Auth, "auth_invalid"); return false; }
  fail(HaError::Parse, "unexpected message during auth");
  return false;
}

bool Flow::result_data(const uint8_t* p, size_t n, bool last) {
  const char* c = reinterpret_cast<const char*>(p);
  if (!decided_) {
    buf_.append(c, n);
    if (buf_.size() < 160 && !last) return true;
    std::string m = strip_spaces(buf_);
    if (m.find("\"type\":\"result\"") == std::string::npos) {
      fail(HaError::Parse, "unexpected message");
      return false;
    }
    decided_ = true;
    if (m.find("\"success\":false") != std::string::npos) {
      errMode_ = true;
      res_.server_error = true;
      if (!last) return true;
    } else {
      bool ok = deliver(buf_.data(), buf_.size());
      buf_.clear();
      if (!ok) return false;
      return true;
    }
  } else if (errMode_) {
    if (buf_.size() < 2048) buf_.append(c, n);
    if (!last) return true;
  } else {
    if (!deliver(c, n)) return false;
    return true;
  }
  // error message complete
  std::string m = strip_spaces(buf_);
  size_t k = m.find("\"code\":\"");
  if (k != std::string::npos) {
    k += 8;
    size_t e = m.find('"', k);
    std::string code = m.substr(k, e == std::string::npos ? std::string::npos : std::min<size_t>(e - k, 64));
    code = scrub(code);
    for (char& c : code)  // HA codes are snake_case words; anything else is not shown
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) c = '?';
    res_.code = code;
  }
  fail(HaError::Http, "ha error " + (res_.code.empty() ? std::string("unknown") : res_.code));
  return false;
}

bool Flow::onData(const uint8_t* p, size_t n, bool first, bool last) {
  (void)first;
  if (aborted()) return false;
  if (phase_ == Phase::Result) {
    if (!result_data(p, n, last)) return false;
    if (last) {
      phase_ = Phase::Done;
      return false;  // stop decoding: we are finished
    }
    return true;
  }
  if (buf_.size() + n > 1024) { fail(HaError::Parse, "oversized control message"); return false; }
  buf_.append(reinterpret_cast<const char*>(p), n);
  if (!last) return true;
  std::string msg;
  msg.swap(buf_);
  return control_message(msg);
}

FlowResult Flow::run() {
  t0_ = sock_.nowMs();
  FlowResult& r = res_;
  auto finish = [&]() {
    r.ms = elapsed();
    return r;
  };
  std::string up = build_upgrade_request(req_.host, req_.key);
  if (sock_.write(reinterpret_cast<const uint8_t*>(up.data()), up.size()) != (int)up.size()) {
    fail(HaError::Network, "write failed");
    sock_.close();
    return finish();
  }
  std::string key = upgrade_key(req_.key);
  FrameDecoder dec(req_.limits.max_bytes);
  std::string head;
  bool upgraded = false;
  uint8_t rb[512];
  while (!aborted() && phase_ != Phase::Done) {
    if (req_.limits.cancel && *req_.limits.cancel) { fail(HaError::Cancelled, "cancelled"); break; }
    uint32_t el = elapsed();
    if (el >= req_.limits.deadline_ms) { fail(HaError::Timeout, "deadline"); break; }
    uint32_t wait = std::min<uint32_t>(req_.limits.deadline_ms - el, 250);
    int n = sock_.read(rb, sizeof rb, wait);
    if (n < 0) { fail(HaError::Network, "closed early"); break; }
    if (n == 0) continue;
    const uint8_t* p = rb;
    size_t len = (size_t)n;
    if (!upgraded) {
      head.append(reinterpret_cast<const char*>(p), len);
      size_t e = head.find("\r\n\r\n");
      if (e == std::string::npos) {
        if (head.size() > 4096) { fail(HaError::Network, "bad upgrade response"); break; }
        continue;
      }
      size_t used = e + 4;
      std::string rest = head.substr(used);
      head.resize(used);
      if (!check_upgrade_response(head, key)) {
        int st = 0;
        if (head.size() > 12) st = atoi(head.c_str() + 9);
        fail(HaError::Network, "upgrade refused (" + std::to_string(st) + ")");
        break;
      }
      upgraded = true;
      if (rest.empty()) continue;
      head = rest;  // reuse as the carry-over buffer
      p = reinterpret_cast<const uint8_t*>(head.data());
      len = head.size();
    }
    FeedStatus s = dec.feed(p, len, *this);
    if (s == FeedStatus::Oversize) fail(HaError::TooLarge, "response too large");
    else if (s == FeedStatus::ProtocolError) fail(HaError::Parse, "websocket protocol error");
    else if (s == FeedStatus::Stopped && phase_ != Phase::Done && !aborted()) fail(HaError::Network, "closed early");
    if (s == FeedStatus::Stopped && head.size() && upgraded) head.clear();
  }
  if (!aborted() && phase_ == Phase::Done) {
    close_now();
  } else {
    sock_.close();
  }
  return finish();
}

}  // namespace

FlowResult run_command(Socket& sock, const Request& req, const ChunkFn& sink) {
  Flow f(sock, req, sink);
  return f.run();
}

}  // namespace ws
