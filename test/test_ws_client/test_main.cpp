#include <unity.h>

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "stats_request.h"
#include "ws_client.h"
#include "ws_crypto.h"
#include "ws_frame.h"

void setUp() {}
void tearDown() {}

using Bytes = std::vector<uint8_t>;

static Bytes sframe(uint8_t op, const std::string& payload, bool fin = true) {
  Bytes f;
  f.push_back((fin ? 0x80 : 0) | op);
  size_t n = payload.size();
  if (n < 126) f.push_back((uint8_t)n);
  else if (n <= 0xFFFF) { f.push_back(126); f.push_back(n >> 8); f.push_back(n & 255); }
  else { f.push_back(127); for (int i = 7; i >= 0; i--) f.push_back((uint64_t)n >> (8 * i)); }
  f.insert(f.end(), payload.begin(), payload.end());
  return f;
}

// ---- crypto --------------------------------------------------------------------------------
void test_accept_rfc_sample() {
  TEST_ASSERT_EQUAL_STRING("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", ws::accept_for_key("dGhlIHNhbXBsZSBub25jZQ==").c_str());
}

void test_sha1_base64() {
  uint8_t h[20];
  ws::sha1(reinterpret_cast<const uint8_t*>("abc"), 3, h);
  TEST_ASSERT_EQUAL_STRING("qZk+NkcGgWq6PiVxeFDCbJzQ2J0=", ws::base64(h, 20).c_str());
  TEST_ASSERT_EQUAL_STRING("", ws::base64(h, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("Zg==", ws::base64(reinterpret_cast<const uint8_t*>("f"), 1).c_str());
  TEST_ASSERT_EQUAL_STRING("Zm8=", ws::base64(reinterpret_cast<const uint8_t*>("fo"), 2).c_str());
  // 56..64 byte messages need two padding blocks
  std::string s(60, 'a');
  ws::sha1(reinterpret_cast<const uint8_t*>(s.data()), s.size(), h);
  TEST_ASSERT_EQUAL_STRING("E9lWAz2a9Em/4sTveMF8IEacS/E=", ws::base64(h, 20).c_str());
}

void test_upgrade_request_and_check() {
  uint8_t key[16] = {'t', 'h', 'e', ' ', 's', 'a', 'm', 'p', 'l', 'e', ' ', 'n', 'o', 'n', 'c', 'e'};
  std::string rq = ws::build_upgrade_request("ha.local:8123", key);
  TEST_ASSERT_TRUE(rq.find("GET /api/websocket HTTP/1.1\r\n") == 0);
  TEST_ASSERT_TRUE(rq.find("Host: ha.local:8123\r\n") != std::string::npos);
  TEST_ASSERT_TRUE(rq.find("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") != std::string::npos);
  TEST_ASSERT_TRUE(rq.find("Sec-WebSocket-Version: 13\r\n") != std::string::npos);
  std::string ok =
      "HTTP/1.1 101 Switching Protocols\r\nupgrade: websocket\r\nConnection: Upgrade\r\n"
      "sec-websocket-accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
  TEST_ASSERT_TRUE(ws::check_upgrade_response(ok, "dGhlIHNhbXBsZSBub25jZQ=="));
  std::string bad = ok;
  bad.replace(bad.find("s3pP"), 4, "XXXX");
  TEST_ASSERT_FALSE(ws::check_upgrade_response(bad, "dGhlIHNhbXBsZSBub25jZQ=="));
  std::string st = ok;
  st.replace(9, 3, "200");
  TEST_ASSERT_FALSE(ws::check_upgrade_response(st, "dGhlIHNhbXBsZSBub25jZQ=="));
}

// ---- frames --------------------------------------------------------------------------------
void test_encode_masked() {
  // RFC 6455 5.7: masked "Hello" with key 37 fa 21 3d
  uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
  Bytes f = ws::encode_frame(ws::kText, reinterpret_cast<const uint8_t*>("Hello"), 5, mask);
  const uint8_t want[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
  TEST_ASSERT_EQUAL(sizeof want, f.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, f.data(), sizeof want);
  Bytes big = ws::encode_frame(ws::kText, Bytes(300, 'a').data(), 300, mask);
  TEST_ASSERT_EQUAL(0x80 | 126, big[1]);
  TEST_ASSERT_EQUAL(300, big[2] << 8 | big[3]);
  TEST_ASSERT_EQUAL(4 + 4 + 300, (int)big.size());
  Bytes c = ws::encode_close(1000, mask);
  TEST_ASSERT_EQUAL(0x88, c[0]);
  TEST_ASSERT_EQUAL(0x82, c[1]);
  TEST_ASSERT_EQUAL(0x03 ^ 0x37, c[6]);
  TEST_ASSERT_EQUAL(0xE8 ^ 0xfa, c[7]);
  TEST_ASSERT_EQUAL(0, ws::encode_frame(ws::kPing, Bytes(126).data(), 126, mask).size());
}

struct Rec : ws::FrameHandler {
  std::string msg;
  std::vector<std::string> msgs;
  std::vector<std::string> pings;
  int closes = 0;
  uint16_t code = 0;
  int firsts = 0, lasts = 0;
  bool stopAfterFirst = false;
  bool onData(const uint8_t* p, size_t n, bool first, bool last) override {
    firsts += first;
    lasts += last;
    msg.append(reinterpret_cast<const char*>(p), n);
    if (last) { msgs.push_back(msg); msg.clear(); }
    return !stopAfterFirst;
  }
  bool onPing(const uint8_t* p, size_t n) override {
    pings.emplace_back(reinterpret_cast<const char*>(p), n);
    return true;
  }
  bool onClose(uint16_t c) override { closes++; code = c; return false; }
};

void test_decode_basic_and_lengths() {
  Rec r;
  ws::FrameDecoder d(1 << 20);
  Bytes a = sframe(ws::kText, "hi");
  std::string l16(200, 'x'), l64(70000, 'y');
  Bytes b = sframe(ws::kText, l16), c = sframe(ws::kText, l64);
  a.insert(a.end(), b.begin(), b.end());
  a.insert(a.end(), c.begin(), c.end());
  TEST_ASSERT_EQUAL(ws::FeedStatus::Ok, d.feed(a.data(), a.size(), r));
  TEST_ASSERT_EQUAL(3, (int)r.msgs.size());
  TEST_ASSERT_EQUAL_STRING("hi", r.msgs[0].c_str());
  TEST_ASSERT_TRUE(r.msgs[1] == l16);
  TEST_ASSERT_TRUE(r.msgs[2] == l64);
  TEST_ASSERT_EQUAL(3, r.firsts);
}

void test_decode_split_every_byte() {
  Rec r;
  ws::FrameDecoder d(1 << 20);
  std::string big(300, 'q');
  Bytes a = sframe(ws::kText, big);
  Bytes p = sframe(ws::kPing, "pp");
  a.insert(a.end(), p.begin(), p.end());
  for (uint8_t byte : a) TEST_ASSERT_EQUAL(ws::FeedStatus::Ok, d.feed(&byte, 1, r));
  TEST_ASSERT_EQUAL(1, (int)r.msgs.size());
  TEST_ASSERT_TRUE(r.msgs[0] == big);
  TEST_ASSERT_EQUAL(1, (int)r.pings.size());
  TEST_ASSERT_EQUAL_STRING("pp", r.pings[0].c_str());
}

void test_decode_fragmented_with_ping() {
  Rec r;
  ws::FrameDecoder d(1 << 20);
  Bytes a = sframe(ws::kText, "ab", false);
  Bytes p = sframe(ws::kPing, "x");
  Bytes b = sframe(ws::kCont, "cd", false);
  Bytes e = sframe(ws::kCont, "", true);  // empty final fragment
  for (Bytes* f : {&p, &b, &e}) a.insert(a.end(), f->begin(), f->end());
  TEST_ASSERT_EQUAL(ws::FeedStatus::Ok, d.feed(a.data(), a.size(), r));
  TEST_ASSERT_EQUAL(1, (int)r.msgs.size());
  TEST_ASSERT_EQUAL_STRING("abcd", r.msgs[0].c_str());
  TEST_ASSERT_EQUAL(1, r.firsts);
  TEST_ASSERT_EQUAL(1, r.lasts);
  TEST_ASSERT_EQUAL(1, (int)r.pings.size());
}

void test_decode_close_and_errors() {
  {
    Rec r;
    ws::FrameDecoder d(1000);
    Bytes c = sframe(ws::kClose, std::string("\x03\xe8", 2));
    TEST_ASSERT_EQUAL(ws::FeedStatus::Stopped, d.feed(c.data(), c.size(), r));
    TEST_ASSERT_EQUAL(1000, r.code);
  }
  {
    Rec r;
    ws::FrameDecoder d(1000);
    Bytes c = sframe(ws::kClose, "");
    TEST_ASSERT_EQUAL(ws::FeedStatus::Stopped, d.feed(c.data(), c.size(), r));
    TEST_ASSERT_EQUAL(1005, r.code);
  }
  {  // continuation without a start
    Rec r;
    ws::FrameDecoder d(1000);
    Bytes c = sframe(ws::kCont, "x");
    TEST_ASSERT_EQUAL(ws::FeedStatus::ProtocolError, d.feed(c.data(), c.size(), r));
  }
  {  // fragmented control frame
    Rec r;
    ws::FrameDecoder d(1000);
    Bytes c = sframe(ws::kPing, "x", false);
    TEST_ASSERT_EQUAL(ws::FeedStatus::ProtocolError, d.feed(c.data(), c.size(), r));
  }
}

void test_decode_oversize() {
  Rec r;
  ws::FrameDecoder d(100);
  Bytes c = sframe(ws::kText, std::string(101, 'a'));
  TEST_ASSERT_EQUAL(ws::FeedStatus::Oversize, d.feed(c.data(), 10, r));  // aborts at the header
  TEST_ASSERT_EQUAL(0, (int)r.msgs.size());
  // fragmented message adding up over the limit
  Rec r2;
  ws::FrameDecoder d2(100);
  Bytes a = sframe(ws::kText, std::string(60, 'a'), false), b = sframe(ws::kCont, std::string(60, 'b'));
  a.insert(a.end(), b.begin(), b.end());
  TEST_ASSERT_EQUAL(ws::FeedStatus::Oversize, d2.feed(a.data(), a.size(), r2));
}

// ---- flow ----------------------------------------------------------------------------------
static const char* kToken = "SECRET-TOKEN-123";

struct Fake : ws::Socket {
  Bytes in;                       // server -> client queue
  size_t pos = 0;
  size_t chunk = 64;              // bytes per read
  uint32_t now = 0, tick = 1;     // clock advance per read
  bool closeWhenEmpty = true;
  bool closed = false;
  Bytes out;                      // client -> server
  std::vector<std::string> texts; // decoded client text frames
  int closeFrames = 0, pongs = 0;
  std::function<void(const std::string&, Fake&)> onText;
  std::string handshakeExtra;     // appended after the 101 head
  bool refuse = false;
  bool upgradeSent = false;

  void push(const Bytes& b) { in.insert(in.end(), b.begin(), b.end()); }
  void pushStr(const std::string& s) { in.insert(in.end(), s.begin(), s.end()); }

  int write(const uint8_t* p, size_t n) override {
    out.insert(out.end(), p, p + n);
    if (!upgradeSent) {
      std::string s(out.begin(), out.end());
      size_t e = s.find("\r\n\r\n");
      if (e == std::string::npos) return (int)n;
      upgradeSent = true;
      size_t k = s.find("Sec-WebSocket-Key: ") + 19;
      std::string key = s.substr(k, s.find("\r\n", k) - k);
      out.erase(out.begin(), out.begin() + e + 4);
      std::string acc = refuse ? "bogus" : ws::accept_for_key(key);
      std::string head = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
              acc + "\r\n\r\n" + handshakeExtra);
      in.insert(in.begin() + pos, head.begin(), head.end());
    }
    decodeClient();
    return (int)n;
  }
  void decodeClient() {
    while (out.size() >= 6) {
      uint8_t op = out[0] & 0x0F;
      size_t len = out[1] & 0x7F, off = 2;
      if (len == 126) { if (out.size() < 4) return; len = out[2] << 8 | out[3]; off = 4; }
      if (out.size() < off + 4 + len) return;
      const uint8_t* m = &out[off];
      std::string pl;
      for (size_t i = 0; i < len; i++) pl += (char)(out[off + 4 + i] ^ m[i & 3]);
      TEST_ASSERT_TRUE_MESSAGE(out[1] & 0x80, "client frame must be masked");
      out.erase(out.begin(), out.begin() + off + 4 + len);
      if (op == ws::kText) { texts.push_back(pl); if (onText) onText(pl, *this); }
      else if (op == ws::kClose) closeFrames++;
      else if (op == ws::kPong) pongs++;
    }
  }
  int read(uint8_t* p, size_t cap, uint32_t timeout_ms) override {
    if (pos >= in.size()) {
      if (closeWhenEmpty) return -1;
      now += timeout_ms;
      return 0;
    }
    now += tick;
    size_t n = std::min({cap, chunk, in.size() - pos});
    memcpy(p, &in[pos], n);
    pos += n;
    return (int)n;
  }
  void close() override { closed = true; }
  uint32_t nowMs() override { return now; }
};

static std::string kResult =
    "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.t\":[{\"start\":1,\"mean\":2.5}]}}";

static void standard_server(Fake& f, const std::string& authReply, const Bytes& resultMsg) {
  f.push(sframe(ws::kText, "{\"type\":\"auth_required\",\"ha_version\":\"2026.1.0\"}"));
  f.onText = [authReply, resultMsg](const std::string& t, Fake& s) {
    if (t.find("\"type\":\"auth\"") != std::string::npos) {
      s.push(sframe(ws::kText, authReply));
    } else if (t.find("statistics_during_period") != std::string::npos) {
      s.push(resultMsg);
    }
  };
}

static ws::Request make_req() {
  ws::Request r;
  r.host = "ha.local";
  r.token = kToken;
  r.command_json = "{\"id\":1,\"type\":\"recorder/statistics_during_period\"}";
  for (int i = 0; i < 16; i++) r.key[i] = (uint8_t)(i * 7 + 1);
  return r;
}

void test_flow_ok() {
  Fake f;
  standard_server(f, "{\"type\":\"auth_ok\",\"ha_version\":\"2026.1.0\"}", sframe(ws::kText, kResult));
  std::string got;
  ws::FlowResult r = ws::run_command(f, make_req(), [&](const char* p, size_t n) { got.append(p, n); return true; });
  TEST_ASSERT_TRUE_MESSAGE(r.ok(), r.message.c_str());
  TEST_ASSERT_TRUE(got == kResult);
  TEST_ASSERT_EQUAL(kResult.size(), r.bytes);
  TEST_ASSERT_EQUAL(2, (int)f.texts.size());
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"auth\",\"access_token\":\"SECRET-TOKEN-123\"}", f.texts[0].c_str());
  TEST_ASSERT_EQUAL(1, f.closeFrames);
  TEST_ASSERT_TRUE(f.closed);
}

void test_flow_ok_one_byte_reads_and_fragments_and_ping() {
  Fake f;
  f.chunk = 1;
  std::string big = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"sensor.t\":[" + std::string(5000, ' ') + "]}}";
  Bytes res = sframe(ws::kText, big.substr(0, 3000), false);
  Bytes p = sframe(ws::kPing, "zz");
  Bytes c = sframe(ws::kCont, big.substr(3000));
  res.insert(res.end(), p.begin(), p.end());
  res.insert(res.end(), c.begin(), c.end());
  standard_server(f, "{\"type\":\"auth_ok\"}", res);
  std::string got;
  ws::FlowResult r = ws::run_command(f, make_req(), [&](const char* q, size_t n) { got.append(q, n); return true; });
  TEST_ASSERT_TRUE_MESSAGE(r.ok(), r.message.c_str());
  TEST_ASSERT_TRUE(got == big);
  TEST_ASSERT_EQUAL(1, f.pongs);
}

void test_flow_handshake_failure() {
  Fake f;
  f.refuse = true;
  ws::FlowResult r = ws::run_command(f, make_req(), nullptr);
  TEST_ASSERT_EQUAL((int)HaError::Network, (int)r.error);
  TEST_ASSERT_TRUE(f.closed);
}

void test_flow_auth_invalid_no_token_leak() {
  Fake f;
  standard_server(f, std::string("{\"type\":\"auth_invalid\",\"message\":\"bad ") + kToken + "\"}", sframe(ws::kText, kResult));
  ws::FlowResult r = ws::run_command(f, make_req(), nullptr);
  TEST_ASSERT_EQUAL((int)HaError::Auth, (int)r.error);
  TEST_ASSERT_TRUE(r.message.find(kToken) == std::string::npos);
  TEST_ASSERT_TRUE(r.code.find(kToken) == std::string::npos);
  TEST_ASSERT_EQUAL(1, (int)f.texts.size());  // no command sent
}

void test_flow_success_false() {
  Fake f;
  std::string err = std::string("{\"id\":1,\"type\":\"result\",\"success\":false,\"error\":{\"code\":\"unknown_command\",\"message\":\"x ") +
                    kToken + "\"}}";
  standard_server(f, "{\"type\":\"auth_ok\"}", sframe(ws::kText, err));
  bool sinkCalled = false;
  ws::FlowResult r = ws::run_command(f, make_req(), [&](const char*, size_t) { sinkCalled = true; return true; });
  TEST_ASSERT_EQUAL((int)HaError::Http, (int)r.error);
  TEST_ASSERT_TRUE(r.server_error);
  TEST_ASSERT_EQUAL_STRING("unknown_command", r.code.c_str());
  TEST_ASSERT_FALSE(sinkCalled);
  TEST_ASSERT_TRUE(r.message.find(kToken) == std::string::npos);
}

void test_flow_unexpected_and_early_close() {
  {
    Fake f;
    f.push(sframe(ws::kText, "{\"type\":\"hello\"}"));
    ws::FlowResult r = ws::run_command(f, make_req(), nullptr);
    TEST_ASSERT_EQUAL((int)HaError::Parse, (int)r.error);
  }
  {  // socket drops after auth_required
    Fake f;
    f.push(sframe(ws::kText, "{\"type\":\"auth_required\"}"));
    ws::FlowResult r = ws::run_command(f, make_req(), nullptr);
    TEST_ASSERT_EQUAL((int)HaError::Network, (int)r.error);
  }
  {  // close frame from the server before the result
    Fake f;
    standard_server(f, "{\"type\":\"auth_ok\"}", sframe(ws::kClose, std::string("\x03\xe8", 2)));
    ws::FlowResult r = ws::run_command(f, make_req(), nullptr);
    TEST_ASSERT_EQUAL((int)HaError::Network, (int)r.error);
  }
  {  // result message cut in the middle
    Fake f;
    Bytes res = sframe(ws::kText, kResult);
    res.resize(res.size() - 10);
    standard_server(f, "{\"type\":\"auth_ok\"}", res);
    ws::FlowResult r = ws::run_command(f, make_req(), [](const char*, size_t) { return true; });
    TEST_ASSERT_EQUAL((int)HaError::Network, (int)r.error);
  }
}

void test_flow_deadline_cancel_toolarge() {
  {
    Fake f;
    f.closeWhenEmpty = false;  // server goes silent after auth_required
    f.push(sframe(ws::kText, "{\"type\":\"auth_required\"}"));
    ws::Request rq = make_req();
    rq.limits.deadline_ms = 2000;
    ws::FlowResult r = ws::run_command(f, rq, nullptr);
    TEST_ASSERT_EQUAL((int)HaError::Timeout, (int)r.error);
  }
  {
    Fake f;
    standard_server(f, "{\"type\":\"auth_ok\"}", sframe(ws::kText, kResult));
    volatile bool cancel = true;
    ws::Request rq = make_req();
    rq.limits.cancel = &cancel;
    ws::FlowResult r = ws::run_command(f, rq, nullptr);
    TEST_ASSERT_EQUAL((int)HaError::Cancelled, (int)r.error);
  }
  {  // sink refuses
    Fake f;
    standard_server(f, "{\"type\":\"auth_ok\"}", sframe(ws::kText, kResult));
    ws::FlowResult r = ws::run_command(f, make_req(), [](const char*, size_t) { return false; });
    TEST_ASSERT_EQUAL((int)HaError::Cancelled, (int)r.error);
  }
  {  // message over the byte limit
    Fake f;
    std::string big = "{\"id\":1,\"type\":\"result\",\"success\":true,\"result\":{\"a\":\"" + std::string(2000, 'x') + "\"}}";
    standard_server(f, "{\"type\":\"auth_ok\"}", sframe(ws::kText, big));
    ws::Request rq = make_req();
    rq.limits.max_bytes = 1000;
    ws::FlowResult r = ws::run_command(f, rq, [](const char*, size_t) { return true; });
    TEST_ASSERT_EQUAL((int)HaError::TooLarge, (int)r.error);
  }
}

void test_statistics_command() {
  std::string c;
  TEST_ASSERT_TRUE(build_statistics_command(1, "sensor.temp_1", 1761696000, 1761699600, StatsPeriod::FiveMinute,
                                            kStatsLineTypes, c));
  TEST_ASSERT_EQUAL_STRING(
      "{\"id\":1,\"type\":\"recorder/statistics_during_period\",\"start_time\":\"2025-10-29T00:00:00+00:00\","
      "\"end_time\":\"2025-10-29T01:00:00+00:00\",\"statistic_ids\":[\"sensor.temp_1\"],\"period\":\"5minute\","
      "\"types\":[\"mean\",\"min\",\"max\"]}",
      c.c_str());
  TEST_ASSERT_TRUE(build_statistics_command(1, "sensor.e", 0, 10, StatsPeriod::Day, stats_bit(StatsType::Change), c));
  TEST_ASSERT_TRUE(c.find("\"types\":[\"change\"]") != std::string::npos);
  TEST_ASSERT_FALSE(build_statistics_command(1, "bad id", 0, 10, StatsPeriod::Day, kStatsLineTypes, c));
  TEST_ASSERT_FALSE(build_statistics_command(1, "sensor.e", 10, 10, StatsPeriod::Day, kStatsLineTypes, c));
  TEST_ASSERT_FALSE(build_statistics_command(1, "sensor.e", 0, 10, StatsPeriod::Day, 0, c));
  TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\\"", json_quote("a\"b\\").c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accept_rfc_sample);
  RUN_TEST(test_sha1_base64);
  RUN_TEST(test_upgrade_request_and_check);
  RUN_TEST(test_encode_masked);
  RUN_TEST(test_decode_basic_and_lengths);
  RUN_TEST(test_decode_split_every_byte);
  RUN_TEST(test_decode_fragmented_with_ping);
  RUN_TEST(test_decode_close_and_errors);
  RUN_TEST(test_decode_oversize);
  RUN_TEST(test_flow_ok);
  RUN_TEST(test_flow_ok_one_byte_reads_and_fragments_and_ping);
  RUN_TEST(test_flow_handshake_failure);
  RUN_TEST(test_flow_auth_invalid_no_token_leak);
  RUN_TEST(test_flow_success_false);
  RUN_TEST(test_flow_unexpected_and_early_close);
  RUN_TEST(test_flow_deadline_cancel_toolarge);
  RUN_TEST(test_statistics_command);
  return UNITY_END();
}
