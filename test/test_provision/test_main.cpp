#include <unity.h>

#include <cstring>
#include <fstream>
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
  TEST_ASSERT_TRUE(nameAllowed("secrets.yaml"));
  TEST_ASSERT_FALSE(nameAllowed("secrets.yaml.tmp"));
  TEST_ASSERT_FALSE(nameAllowed("Secrets.yaml"));
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

void test_secrets_summary() {
  auto ok = config::parseSecrets(std::string(
      "wifi:\n  ssid: SECRETssid\nhome_assistant:\n  internal_url: http://SECRETh\n  token: SECRETtok\n"));
  const std::string s = secretsSummary(ok);
  TEST_ASSERT_EQUAL_STRING("PUT-SECRETS ok wifi=yes ha=yes", s.c_str());
  auto bad = config::parseSecrets(std::string("wifi:\n  password: SECRETpw\n"));
  const std::string e = secretsSummary(bad);
  TEST_ASSERT_EQUAL(0, (int)e.rfind("PUT-SECRETS error count=", 0));
  TEST_ASSERT_TRUE(e.find("SECRETpw") == std::string::npos);
}

// Output of site/config-check.js buildSecretsYaml (JSON-quoted scalars) for hostile values must round-trip
// through the firmware parser unchanged (task 119 review).
void test_builder_secrets_roundtrip() {
  struct C { const char *yaml; const char *ssid; const char *pw; };
  const C cases[] = {
    {"wifi:\n  - ssid: \"net0 #x\"\n    password: \"p\\\"q\\\\r#x: y\\nz\"\nhome_assistant:\n  internal_url: \"http://192.168.1.10:8123\"\n  token: \"t.o-k_0\"\n",
     "net0 #x", "p\"q\\r#x: y\nz"},
    {"wifi:\n  - ssid: \"net1 #x\"\n    password: \"it's #1 - [a]{b}\"\nhome_assistant:\n  internal_url: \"http://192.168.1.10:8123\"\n  token: \"t.o-k_1\"\n",
     "net1 #x", "it's #1 - [a]{b}"},
    {"wifi:\n  - ssid: \"net2 #x\"\n    password: \"tab\\there\"\nhome_assistant:\n  internal_url: \"http://192.168.1.10:8123\"\n  token: \"t.o-k_2\"\n",
     "net2 #x", "tab\there"},
    {"wifi:\n  - ssid: \"net3 #x\"\n    password: \"\xc3\xbcn\xc3\xaf \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80\"\nhome_assistant:\n  internal_url: \"http://192.168.1.10:8123\"\n  token: \"t.o-k_3\"\n",
     "net3 #x", "\xc3\xbcn\xc3\xaf \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80"},
  };
  for (const C &c : cases) {
    auto r = config::parseSecrets(std::string(c.yaml));
    TEST_ASSERT_TRUE_MESSAGE(r.ok(), c.yaml);
    TEST_ASSERT_EQUAL_STRING(c.ssid, r.connection.wifi.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(c.pw, r.connection.wifi.password.c_str());
  }
}

static std::string unhex(const std::string &h) {
  std::string o;
  for (size_t i = 0; i + 1 < h.size(); i += 2) o += char(std::stoi(h.substr(i, 2), nullptr, 16));
  return o;
}

// Live round trip: site/test/gen-secrets-fixture.js writes the builder's output for more hostile passwords;
// each record is "<ssid hex> <password hex> <yaml hex>".
void test_builder_secrets_roundtrip_fixture() {
  std::ifstream f("test/test_provision/secrets_roundtrip.txt");
  TEST_ASSERT_TRUE_MESSAGE(f.is_open(), "fixture missing: run node site/test/gen-secrets-fixture.js");
  std::string line;
  int n = 0;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    size_t a = line.find(' '), b = line.find(' ', a + 1);
    TEST_ASSERT_TRUE(a != std::string::npos && b != std::string::npos);
    const std::string ssid = unhex(line.substr(0, a)), pw = unhex(line.substr(a + 1, b - a - 1)), y = unhex(line.substr(b + 1));
    auto r = config::parseSecrets(y);
    TEST_ASSERT_TRUE_MESSAGE(r.ok(), ("record " + std::to_string(n)).c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE(ssid.c_str(), r.connection.wifi.ssid.c_str(), ("ssid " + std::to_string(n)).c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE(pw.c_str(), r.connection.wifi.password.c_str(), ("password " + std::to_string(n)).c_str());
    ++n;
  }
  TEST_ASSERT_TRUE(n >= 20);
}

void test_getfile_config_only() {
  std::string n;
  TEST_ASSERT_TRUE(parseGetfile("getfile config.yaml", n) == GetStatus::Ok);
  TEST_ASSERT_EQUAL_STRING("config.yaml", n.c_str());
  TEST_ASSERT_TRUE(parseGetfile("putfile config.yaml 5 00000000", n) == GetStatus::NotGetfile);
  TEST_ASSERT_TRUE(parseGetfile("getfiles", n) == GetStatus::NotGetfile);
  TEST_ASSERT_TRUE(parseGetfile("getfile", n) == GetStatus::BadCommand);
  TEST_ASSERT_TRUE(parseGetfile("getfile ", n) == GetStatus::BadCommand);
  TEST_ASSERT_TRUE(parseGetfile("getfile config.yaml extra", n) == GetStatus::BadCommand);
}

// Secrets must be unreadable: every name except config.yaml is refused, whatever the spelling.
void test_getfile_refuses_secrets() {
  const char *names[] = {"secrets.yaml", "/secrets.yaml", "Secrets.yaml", "SECRETS.YAML", "secrets.yaml.tmp",
                         "config.yaml.tmp", "../secrets.yaml", "./config.yaml", "/config.yaml", "config.yaml/",
                         "test-x.yaml", "secrets", "config.yaml\\0", "CONFIG.YAML", "config.yml", ""};
  for (const char *nm : names) {
    std::string n = "untouched";
    const GetStatus s = parseGetfile(std::string("getfile ") + nm, n);
    TEST_ASSERT_TRUE_MESSAGE(s != GetStatus::Ok, nm);
    TEST_ASSERT_EQUAL_STRING("untouched", n.c_str());
    TEST_ASSERT_FALSE_MESSAGE(getNameAllowed(nm), nm);
  }
  std::string n;
  TEST_ASSERT_EQUAL_STRING("bad-name", getStatusReason(parseGetfile("getfile secrets.yaml", n)));
  TEST_ASSERT_EQUAL_STRING("bad-command", getStatusReason(parseGetfile("getfile", n)));
  TEST_ASSERT_TRUE(getNameAllowed("config.yaml"));
}

void test_get_begin() {
  TEST_ASSERT_EQUAL_STRING("GET-BEGIN 1204 0000ff0a", getBegin(1204, 0xff0a).c_str());
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
  RUN_TEST(test_secrets_summary);
  RUN_TEST(test_builder_secrets_roundtrip);
  RUN_TEST(test_builder_secrets_roundtrip_fixture);
  RUN_TEST(test_getfile_config_only);
  RUN_TEST(test_getfile_refuses_secrets);
  RUN_TEST(test_get_begin);
  return UNITY_END();
}
