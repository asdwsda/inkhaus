#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "provision_proto.h"

using namespace provision;

void setUp() {}
void tearDown() {}

static const uint8_t *B(const std::string &s) { return reinterpret_cast<const uint8_t *>(s.data()); }

void test_crc32_known() {
  const std::string s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32(B(s), s.size()));
  TEST_ASSERT_EQUAL_HEX32(0u, crc32(nullptr, 0));
  uint32_t c = crc32Update(0, B(s), 4);
  c = crc32Update(c, B(s) + 4, 5);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, c);
}

void test_name_whitelist() {
  TEST_ASSERT_TRUE(nameAllowed("config.yaml"));
  TEST_ASSERT_TRUE(nameAllowed("test-provision.yaml"));
  TEST_ASSERT_TRUE(nameAllowed("test-a_b.1-2.yaml"));
  TEST_ASSERT_FALSE(nameAllowed(""));
  TEST_ASSERT_FALSE(nameAllowed("test-.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("config.yml"));
  TEST_ASSERT_FALSE(nameAllowed("secrets.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("/config.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("../config.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("test-../x.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("test-a b.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("config.yaml.tmp"));
  TEST_ASSERT_FALSE(nameAllowed("test-" + std::string(60, 'a') + ".yaml"));
}

void test_parse_ok() {
  Request r;
  TEST_ASSERT_EQUAL(ParseStatus::Ok, parsePutfile("putfile config.yaml 1234 CBF43926", r));
  TEST_ASSERT_EQUAL_STRING("config.yaml", r.name.c_str());
  TEST_ASSERT_EQUAL_UINT32(1234, r.size);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, r.crc);
  TEST_ASSERT_EQUAL(ParseStatus::Ok, parsePutfile("putfile test-x.yaml 65536 0000abcd", r));
  TEST_ASSERT_EQUAL_UINT32(65536, r.size);
  TEST_ASSERT_EQUAL_HEX32(0xabcdu, r.crc);
}

void test_parse_errors() {
  Request r;
  TEST_ASSERT_EQUAL(ParseStatus::NotPutfile, parsePutfile("shot", r));
  TEST_ASSERT_EQUAL(ParseStatus::NotPutfile, parsePutfile("putfiles a 1 2", r));
  TEST_ASSERT_EQUAL(ParseStatus::NotPutfile, parsePutfile("", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadCommand, parsePutfile("putfile", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadCommand, parsePutfile("putfile config.yaml 10", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadCommand, parsePutfile("putfile config.yaml 10 00000000 x", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadName, parsePutfile("putfile evil.sh 10 00000000", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadSize, parsePutfile("putfile config.yaml 65537 00000000", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadSize, parsePutfile("putfile config.yaml 0 00000000", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadSize, parsePutfile("putfile config.yaml -5 00000000", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadSize, parsePutfile("putfile config.yaml 99999999999 00000000", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadCrc, parsePutfile("putfile config.yaml 10 xyz", r));
  TEST_ASSERT_EQUAL(ParseStatus::BadCrc, parsePutfile("putfile config.yaml 10 0000000g", r));
  TEST_ASSERT_EQUAL_STRING("bad-size", parseStatusReason(ParseStatus::BadSize));
}

static std::string payload(size_t n) {
  std::string s(n, 'a');
  for (size_t i = 0; i < n; ++i) s[i] = char('a' + (i * 7) % 26);
  return s;
}

void test_receiver_acks_every_kb() {
  const std::string d = payload(2500);
  Request req{"test-x.yaml", 2500, crc32(B(d), d.size())};
  Receiver rc;
  rc.begin(req, 0);
  std::vector<uint32_t> acks;
  const uint8_t *p = B(d);
  size_t n = d.size();
  bool complete = false;
  uint32_t now = 0;
  while (n) {  // whole buffer in one go: feed must split at the 1 KB boundaries
    auto r = rc.feed(p, n, ++now);
    TEST_ASSERT_TRUE(r.consumed > 0);
    p += r.consumed;
    n -= r.consumed;
    if (r.ack) acks.push_back(r.ackOffset);
    complete = r.complete;
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL(2, (int)acks.size());
  TEST_ASSERT_EQUAL_UINT32(1024, acks[0]);
  TEST_ASSERT_EQUAL_UINT32(2048, acks[1]);
  TEST_ASSERT_EQUAL(Verdict::Ok, rc.verdict());
}

void test_receiver_exact_multiple_has_no_final_ack() {
  const std::string d = payload(2048);
  Request req{"config.yaml", 2048, crc32(B(d), d.size())};
  Receiver rc;
  rc.begin(req, 0);
  int acks = 0;
  const uint8_t *p = B(d);
  size_t n = d.size();
  while (n) {
    auto r = rc.feed(p, n, 1);
    p += r.consumed;
    n -= r.consumed;
    acks += r.ack;
  }
  TEST_ASSERT_EQUAL(1, acks);  // 1024 only; the end is answered by PUT-OK/PUT-ERR
  TEST_ASSERT_TRUE(rc.complete());
}

void test_receiver_small_pieces_and_extra_bytes() {
  const std::string d = payload(300) + "TRAILING";
  Request req{"config.yaml", 300, crc32(B(d), 300)};
  Receiver rc;
  rc.begin(req, 0);
  size_t off = 0;
  while (off < d.size()) {
    size_t piece = d.size() - off > 100 ? 100 : d.size() - off;
    auto r = rc.feed(B(d) + off, piece, 5);
    off += r.consumed;
    if (r.consumed == 0) break;  // trailing bytes are not taken
  }
  TEST_ASSERT_EQUAL_UINT32(300, rc.received());
  TEST_ASSERT_EQUAL(Verdict::Ok, rc.verdict());
  TEST_ASSERT_EQUAL(0, (int)rc.feed(B(d), 5, 6).consumed);
}

void test_receiver_crc_mismatch() {
  const std::string d = payload(100);
  Request req{"config.yaml", 100, crc32(B(d), d.size()) ^ 1u};
  Receiver rc;
  rc.begin(req, 0);
  TEST_ASSERT_EQUAL(Verdict::Incomplete, rc.verdict());
  rc.feed(B(d), 100, 1);
  TEST_ASSERT_EQUAL(Verdict::CrcMismatch, rc.verdict());
}

void test_receiver_timeout() {
  Request req{"config.yaml", 100, 0};
  Receiver rc;
  rc.begin(req, 1000);
  TEST_ASSERT_FALSE(rc.timedOut(1000 + kIdleTimeoutMs));
  TEST_ASSERT_TRUE(rc.timedOut(1000 + kIdleTimeoutMs + 1));
  const std::string d = payload(10);
  rc.feed(B(d), 10, 3000);  // activity restarts the clock
  TEST_ASSERT_FALSE(rc.timedOut(3000 + kIdleTimeoutMs));
  TEST_ASSERT_TRUE(rc.timedOut(3000 + kIdleTimeoutMs + 1));
}

void test_receiver_timeout_wraps() {
  Request req{"config.yaml", 100, 0};
  Receiver rc;
  rc.begin(req, 0xFFFFFF00u);
  TEST_ASSERT_FALSE(rc.timedOut(0x100u));
  TEST_ASSERT_TRUE(rc.timedOut(0xFFFFFF00u + kIdleTimeoutMs + 1));
}

void test_hex32() {
  TEST_ASSERT_EQUAL_STRING("0000abcd", hex32(0xabcd).c_str());
  TEST_ASSERT_EQUAL_STRING("cbf43926", hex32(0xCBF43926u).c_str());
}

void test_summary_ok_has_no_values() {
  const char *yaml =
      "version: 1\n"
      "connection:\n"
      "  wifi:\n"
      "    ssid: SECRET-SSID\n"
      "    password: SECRET-PASS\n"
      "  home_assistant:\n"
      "    internal_url: http://192.168.1.2:8123\n"
      "    token: SECRET-TOKEN\n"
      "tabs:\n"
      "  - title: A\n"
      "    widgets:\n"
      "      - entity: light.one\n"
      "      - entity: switch.two\n"
      "  - title: B\n"
      "    widgets:\n"
      "      - entity: sensor.three\n";
  auto r = config::parseConfig(std::string(yaml));
  const std::string s = configSummary(r);
  TEST_ASSERT_EQUAL_STRING("PUT-CONFIG ok tabs=2 widgets=3 connection=yes", s.c_str());
  TEST_ASSERT_TRUE(s.find("SECRET") == std::string::npos);
}

void test_summary_error() {
  auto r = config::parseConfig(std::string("version: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: nodot\n"));
  TEST_ASSERT_FALSE(r.ok());
  const std::string s = configSummary(r);
  TEST_ASSERT_EQUAL(0, (int)s.rfind("PUT-CONFIG error count=", 0));
  TEST_ASSERT_TRUE(s.find(" first=") != std::string::npos);
  TEST_ASSERT_TRUE(s.find(" line=") != std::string::npos);
  TEST_ASSERT_TRUE(s.find("nodot") == std::string::npos);
}

void test_summary_garbage() {
  auto r = config::parseConfig(std::string("\x01\x02: [unclosed"));
  const std::string s = configSummary(r);
  TEST_ASSERT_EQUAL(0, (int)s.rfind("PUT-CONFIG error", 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_known);
  RUN_TEST(test_name_whitelist);
  RUN_TEST(test_parse_ok);
  RUN_TEST(test_parse_errors);
  RUN_TEST(test_receiver_acks_every_kb);
  RUN_TEST(test_receiver_exact_multiple_has_no_final_ack);
  RUN_TEST(test_receiver_small_pieces_and_extra_bytes);
  RUN_TEST(test_receiver_crc_mismatch);
  RUN_TEST(test_receiver_timeout);
  RUN_TEST(test_receiver_timeout_wraps);
  RUN_TEST(test_hex32);
  RUN_TEST(test_summary_ok_has_no_values);
  RUN_TEST(test_summary_error);
  RUN_TEST(test_summary_garbage);
  return UNITY_END();
}
