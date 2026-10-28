#include <unity.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "config_model.h"

using namespace config;

void setUp() {}
void tearDown() {}

namespace {

std::string readFile(const char *path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Minimal valid config; tests append or replace parts.
const char *kMinimal =
    "version: 1\n"
    "tabs:\n"
    "  - title: P\n"
    "    widgets:\n"
    "      - entity: light.a\n";

const ConfigError *findError(const ParseResult &r, const std::string &path) {
  for (const auto &e : r.errors)
    if (e.path == path) return &e;
  return nullptr;
}

void assertErrorAt(const std::string &yaml, const std::string &path) {
  ParseResult r = parseConfig(yaml);
  std::string all;
  for (const auto &e : r.errors) all += e.path + ": " + e.message + "\n";
  TEST_ASSERT_NOT_NULL_MESSAGE(findError(r, path), all.c_str());
}

void assertSyntaxError(const std::string &yaml) {
  ParseResult r = parseConfig(yaml);
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_UINT(1, r.errors.size());
  TEST_ASSERT_GREATER_THAN_INT(0, r.errors[0].line);
}

}  // namespace

void test_example_parses_clean() {
  std::string text = readFile("config.example.yaml");
  TEST_ASSERT_FALSE_MESSAGE(text.empty(), "run tests from the repo root");
  ParseResult r = parseConfig(text);
  std::string all;
  for (const auto &e : r.errors) all += e.path + ": " + e.message + "\n";
  TEST_ASSERT_TRUE_MESSAGE(r.ok(), all.c_str());

  const Config &c = r.config;
  TEST_ASSERT_EQUAL_INT(1, c.version);
  TEST_ASSERT_FALSE(c.hasConnection);  // the example config holds only the dashboard
  SecretsResult sr = parseSecrets(readFile("secrets.example.yaml"));
  std::string serr;
  for (const auto &e : sr.errors) serr += e.path + ": " + e.message + "\n";
  TEST_ASSERT_TRUE_MESSAGE(sr.ok(), serr.c_str());
  const ConnectionConfig &cn = sr.connection;
  TEST_ASSERT_EQUAL_STRING("MyWifi", cn.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secret", cn.wifi.password.c_str());
  TEST_ASSERT_TRUE(cn.wifi.hasStaticIp);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", cn.wifi.staticIp.ip.c_str());
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", cn.wifi.staticIp.subnet.c_str());
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.10:8123", cn.ha.internalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("https://example.ui.nabu.casa", cn.ha.externalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("long-lived-token", cn.ha.token.c_str());

  TEST_ASSERT_EQUAL_UINT(2, c.groups.size());
  TEST_ASSERT_EQUAL_STRING("downstairs_shutters", c.groups[0].name.c_str());
  TEST_ASSERT_EQUAL_UINT(3, c.groups[0].members.size());

  TEST_ASSERT_EQUAL_UINT(2, c.tabs.size());
  TEST_ASSERT_EQUAL_STRING("Living room", c.tabs[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Whole house", c.tabs[1].title.c_str());
  const auto &w = c.tabs[0].widgets;
  TEST_ASSERT_EQUAL_STRING("Shutter", w[0].name.c_str());
  TEST_ASSERT_FALSE(w[0].size.has_value());
  TEST_ASSERT_TRUE(w[1].type == WidgetType::Cover);
  TEST_ASSERT_EQUAL_STRING("downstairs_shutters", w[1].group.c_str());
  TEST_ASSERT_EQUAL_UINT(3, w[1].entities.size());
  // The example sets no size: the per-orientation size table (layout.md section 3) allows
  // no other size for these types, so any value would be clamped.
  TEST_ASSERT_FALSE(w[1].size.has_value());
  for (const auto &t : c.tabs)
    for (const auto &wd : t.widgets) TEST_ASSERT_FALSE(wd.size.has_value());

  // Every widget type appears at least once across the tabs.
  bool seen[9] = {};
  for (const auto &t : c.tabs)
    for (const auto &wd : t.widgets) seen[static_cast<int>(wd.type)] = true;
  for (int i = 0; i < 9; ++i) TEST_ASSERT_TRUE_MESSAGE(seen[i], "widget type missing from example");
}

const char *kSecrets =
    "wifi:\n  ssid: SECRETssid\n  password: SECRETpw\n"
    "home_assistant:\n  internal_url: http://10.0.0.2:8123\n  token: SECRETtoken\n";
const char *kConnCfg =
    "version: 1\nconnection:\n  wifi:\n    ssid: cfgssid\n  home_assistant:\n"
    "    internal_url: http://10.0.0.3:8123\n    token: cfgtoken\n"
    "tabs:\n  - title: P\n    widgets:\n      - entity: light.a\n";

void test_secrets_parse_ok() {
  SecretsResult r = parseSecrets(kSecrets);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL_STRING("SECRETssid", r.connection.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("SECRETtoken", r.connection.ha.token.c_str());
  TEST_ASSERT_FALSE(r.connection.wifi.hasStaticIp);
}

void test_secrets_errors_path_line_no_values() {
  SecretsResult r = parseSecrets(
      "wifi:\n  password: SECRETpw\n    static_ip: x\n"
      "home_assistant:\n  internal_url: ftp://SECRETurl\n  token: SECRETtoken\n");
  TEST_ASSERT_FALSE(r.ok());
  r = parseSecrets(
      "wifi:\n  password: SECRETpw\n"
      "home_assistant:\n  internal_url: ftp://SECRETurl\n  token: SECRETtoken\n");
  TEST_ASSERT_FALSE(r.ok());
  bool ssid = false, url = false;
  for (const auto &e : r.errors) {
    ssid |= e.path == "wifi.ssid" && e.line > 0;
    url |= e.path == "home_assistant.internal_url" && e.line == 4;
    TEST_ASSERT_NULL(strstr(e.message.c_str(), "SECRET"));
    TEST_ASSERT_NULL(strstr(e.path.c_str(), "SECRET"));
  }
  TEST_ASSERT_TRUE(ssid);
  TEST_ASSERT_TRUE(url);
}

void test_secrets_missing_sections_and_top_level() {
  SecretsResult r = parseSecrets("wifi:\n  ssid: a\n");
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_STRING("home_assistant", r.errors[0].path.c_str());
  TEST_ASSERT_FALSE(parseSecrets("- a\n- b\n").ok());
  TEST_ASSERT_FALSE(parseSecrets("", 0).ok());
  TEST_ASSERT_FALSE(parseSecrets("wifi: [\n").ok());
}

void test_secrets_static_ip_paths() {
  SecretsResult r = parseSecrets(
      "wifi:\n  ssid: a\n  static_ip:\n    ip: 192.168.1.5\n"
      "home_assistant:\n  internal_url: http://h\n  token: t\n");
  TEST_ASSERT_FALSE(r.ok());
  bool found = false;
  for (const auto &e : r.errors) found |= e.path == "wifi.static_ip.subnet";
  TEST_ASSERT_TRUE(found);
}

void test_merge_secrets_win() {
  SecretsResult s = parseSecrets(kSecrets);
  MergeOutcome m = mergeSecrets(parseConfig(kMinimal), &s);
  TEST_ASSERT_TRUE(m.parse.ok());
  TEST_ASSERT_FALSE(m.warnConnectionInConfig);
  TEST_ASSERT_TRUE(m.parse.config.hasConnection);
  TEST_ASSERT_EQUAL_STRING("SECRETssid", m.parse.config.connection.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_UINT(1, m.parse.config.tabs.size());
}

void test_merge_connection_in_config_rejected_with_secrets() {
  SecretsResult s = parseSecrets(kSecrets);
  MergeOutcome m = mergeSecrets(parseConfig(kConnCfg), &s);
  TEST_ASSERT_FALSE(m.parse.ok());
  TEST_ASSERT_NOT_NULL(findError(m.parse, "connection"));
  TEST_ASSERT_NOT_NULL(strstr(m.parse.errors[0].message.c_str(), "secrets.yaml"));
}

void test_merge_secrets_errors_prefixed() {
  SecretsResult s = parseSecrets("wifi:\n  password: x\n");
  MergeOutcome m = mergeSecrets(parseConfig(kMinimal), &s);
  TEST_ASSERT_FALSE(m.parse.ok());
  TEST_ASSERT_NOT_NULL(findError(m.parse, "secrets.yaml: wifi.ssid"));
}

void test_merge_without_secrets_keeps_connection_with_warning() {
  MergeOutcome m = mergeSecrets(parseConfig(kConnCfg), nullptr);
  TEST_ASSERT_TRUE(m.parse.ok());
  TEST_ASSERT_TRUE(m.warnConnectionInConfig);
  TEST_ASSERT_EQUAL_STRING("cfgssid", m.parse.config.connection.wifi.ssid.c_str());
  m = mergeSecrets(parseConfig(kMinimal), nullptr);
  TEST_ASSERT_TRUE(m.parse.ok());
  TEST_ASSERT_FALSE(m.warnConnectionInConfig);
  TEST_ASSERT_FALSE(m.parse.config.hasConnection);
}

void test_merge_config_errors_kept() {
  SecretsResult s = parseSecrets(kSecrets);
  MergeOutcome m = mergeSecrets(parseConfig("version: 1\n"), &s);
  TEST_ASSERT_FALSE(m.parse.ok());
  TEST_ASSERT_NOT_NULL(findError(m.parse, "tabs"));
}

void test_connection_absent() {
  ParseResult r = parseConfig(kMinimal);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_FALSE(r.config.hasConnection);
  TEST_ASSERT_EQUAL_UINT(1, r.config.tabs.size());
}

void test_timezone_default_and_valid() {
  ParseResult r = parseConfig(kMinimal);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", r.config.timezone.c_str());
  r = parseConfig(std::string("timezone: \"EST5EDT,M3.2.0,M11.1.0\"\n") + kMinimal);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", r.config.timezone.c_str());
}

void test_timezone_invalid() {
  for (const char *bad : {"Europe/Budapest", "\"CET -1\"", "\"CET\"", "\"1CET-1\"", "5", "[a]"}) {
    ParseResult r = parseConfig(std::string("timezone: ") + bad + "\n" + kMinimal);
    TEST_ASSERT_FALSE_MESSAGE(r.ok(), bad);
    TEST_ASSERT_NOT_NULL_MESSAGE(findError(r, "timezone"), bad);
  }
}

void test_error_missing_tabs() { assertErrorAt("version: 1\n", "tabs"); }

void test_pages_key_is_unknown() {
  ParseResult r = parseConfig(std::string("version: 1\npages:\n  - title: P\n"
                                          "    widgets:\n      - entity: light.a\n"));
  TEST_ASSERT_FALSE(r.ok());
  const ConfigError *e = findError(r, "tabs");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_STRING("missing required field", e->message.c_str());
}

void test_size_valid() {
  std::string y = "version: 1\ntabs:\n  - title: P\n    widgets:\n"
                  "      - entity: light.a\n        size: 2x1\n"
                  "      - entity: light.b\n        size: 4x4\n"
                  "      - entity: light.c\n        size: 1x3\n"
                  "      - entity: light.d\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_TRUE(r.ok());
  const auto &w = r.config.tabs[0].widgets;
  TEST_ASSERT_EQUAL_INT(2, w[0].size->w);
  TEST_ASSERT_EQUAL_INT(1, w[0].size->h);
  TEST_ASSERT_EQUAL_INT(4, w[1].size->w);
  TEST_ASSERT_EQUAL_INT(4, w[1].size->h);
  TEST_ASSERT_EQUAL_INT(3, w[2].size->h);
  TEST_ASSERT_FALSE(w[3].size.has_value());
}

void test_size_invalid() {
  const char *bad[] = {"0x1", "5x1", "2x", "abc", "1x0", "1x5", "x2", "2X1", "2x1x1", "22x1", "2"};
  for (const char *b : bad) {
    std::string y = std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n"
                                "      - entity: light.a\n      - entity: light.b\n"
                                "        size: \"") + b + "\"\n";
    ParseResult r = parseConfig(y);
    const ConfigError *e = findError(r, "tabs[0].widgets[1].size");
    TEST_ASSERT_NOT_NULL_MESSAGE(e, b);
    TEST_ASSERT_EQUAL_INT(7, e->line);
  }
}

void test_error_group_name_charset() {
  assertErrorAt("version: 1\ngroups:\n  a.b: [light.a]\ntabs:\n  - title: P\n"
                "    widgets:\n      - entity: light.a\n",
                "groups.a.b");
  assertErrorAt("version: 1\ngroups:\n  Upstairs: [light.a]\ntabs:\n  - title: P\n"
                "    widgets:\n      - entity: light.a\n",
                "groups.Upstairs");
  assertErrorAt("version: 1\ngroups:\n  my-group: [light.a]\ntabs:\n  - title: P\n"
                "    widgets:\n      - entity: light.a\n",
                "groups.my-group");
  TEST_ASSERT_TRUE(parseConfig(std::string("version: 1\ngroups:\n  g_1: [light.a]\ntabs:\n"
                                           "  - title: P\n    widgets:\n      - entity: light.a\n"))
                       .ok());
}

void test_error_entity_and_group() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a]\ntabs:\n  - title: P\n"
                            "    widgets:\n      - entity: light.a\n        group: g\n"
                            "        name: n\n"),
                "tabs[0].widgets[0]");
}

void test_error_unknown_group() {
  assertErrorAt(std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n"
                            "      - group: nope\n        name: n\n"),
                "tabs[0].widgets[0].group");
}

void test_error_group_mixed_domains() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a, cover.b]\ntabs:\n"
                            "  - title: P\n    widgets:\n      - group: g\n        name: n\n"),
                "groups.g");
}

void test_error_unsupported_domain() {
  assertErrorAt(std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n"
                            "      - entity: person.bob\n"),
                "tabs[0].widgets[0].entity");
  // With explicit type it is accepted.
  ParseResult r = parseConfig(std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n"
                                          "      - entity: person.bob\n        type: sensor\n"));
  TEST_ASSERT_TRUE(r.ok());
}

// Error messages must never echo a value from the file (URLs, SSIDs, tokens, names).
void test_error_messages_do_not_echo_values() {
  const char *yamls[] = {
      "version: SECRETv\ntabs:\n  - title: P\n    widgets:\n      - entity: light.a\n",
      "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: light.a\n        size: SECRETs\n",
      "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: light.a\n        type: SECRETt\n",
      "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: secretd.x\n",
      "version: 1\ntabs:\n  - title: P\n    widgets:\n      - group: secretg\n        name: n\n",
      "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: SECRETe\n",
      "version: 1\nconnection:\n  wifi:\n    ssid: SECRETssid\n  home_assistant:\n"
      "    internal_url: SECRETurl\n    external_url: http://SECREText\n    token: SECRETtok\n"
      "tabs:\n  - title: P\n    widgets:\n      - entity: light.a\n",
  };
  for (const char *y : yamls) {
    ParseResult r = parseConfig(std::string(y));
    TEST_ASSERT_FALSE(r.ok());
    for (const auto &e : r.errors) {
      std::string m = e.message;
      for (auto &c : m) c = (char)tolower((unsigned char)c);
      TEST_ASSERT_TRUE_MESSAGE(m.find("secret") == std::string::npos, e.message.c_str());
    }
  }
}

void test_error_external_url_not_https() {
  std::string y =
      "version: 1\nconnection:\n  wifi:\n    ssid: a\n  home_assistant:\n"
      "    internal_url: http://x:8123\n    external_url: http://y\n    token: t\n"
      "tabs:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  assertErrorAt(y, "connection.home_assistant.external_url");
}

void test_error_partial_static_ip() {
  std::string y =
      "version: 1\nconnection:\n  wifi:\n    ssid: a\n    static_ip:\n      ip: 192.168.1.5\n"
      "      gateway: 192.168.1.1\n  home_assistant:\n    internal_url: http://x\n    token: t\n"
      "tabs:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_NOT_NULL(findError(r, "connection.wifi.static_ip.subnet"));
  TEST_ASSERT_NOT_NULL(findError(r, "connection.wifi.static_ip.dns"));
  TEST_ASSERT_TRUE(findError(r, "connection.wifi.static_ip.dns")->line > 0);
}

void test_error_wrong_version() {
  ParseResult r = parseConfig(std::string("version: 2\ntabs:\n  - title: P\n    widgets: []\n"));
  const ConfigError *e = findError(r, "version");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(1, e->line);
}

void test_error_group_widget_needs_name() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a]\ntabs:\n  - title: P\n"
                            "    widgets:\n      - group: g\n"),
                "tabs[0].widgets[0].name");
}

void test_error_line_number_of_validation() {
  ParseResult r = parseConfig(std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n"
                                          "      - entity: light.a\n      - entity: person.x\n"));
  const ConfigError *e = findError(r, "tabs[0].widgets[1].entity");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(6, e->line);
}

void test_syntax_bad_indentation() {
  assertSyntaxError("version: 1\ntabs:\n  - title: P\n   widgets: []\n");
}

void test_syntax_tabs() {
  ParseResult r = parseConfig(std::string("version: 1\ntabs:\n\t- title: P\n"));
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_INT(3, r.errors[0].line);
}

void test_syntax_truncated() {
  assertSyntaxError("version: 1\ntabs:\n  - title: \"Living ro");
  assertSyntaxError("version: 1\ngroups:\n  g: [light.a, light.b\n");
}

void test_empty_and_garbage() {
  TEST_ASSERT_FALSE(parseConfig(std::string("")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("just a string")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("- a\n- b\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("a: &x 1\nb: *x\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("\x01\xff\xfe: [\n")).ok());
}

void test_limit_tabs() {
  std::string y = "version: 1\ntabs:\n";
  for (int i = 0; i < 17; ++i) y += "  - title: P\n    widgets:\n      - entity: light.a\n";
  TEST_ASSERT_NOT_NULL(findError(parseConfig(y), "tabs"));
  y = "version: 1\ntabs:\n";
  for (int i = 0; i < 16; ++i) y += "  - title: P\n    widgets:\n      - entity: light.a\n";
  TEST_ASSERT_TRUE(parseConfig(y).ok());
}

void test_limit_widgets() {
  std::string head = "version: 1\ntabs:\n  - title: P\n    widgets:\n";
  std::string y = head;
  for (int i = 0; i < 25; ++i) y += "      - entity: light.a" + std::to_string(i) + "\n";
  TEST_ASSERT_NOT_NULL(findError(parseConfig(y), "tabs[0].widgets"));
  y = head;
  for (int i = 0; i < 24; ++i) y += "      - entity: light.a" + std::to_string(i) + "\n";
  TEST_ASSERT_TRUE(parseConfig(y).ok());
}

void test_limit_group_entities() {
  std::string y = "version: 1\ngroups:\n  g: [";
  for (int i = 0; i < 17; ++i) y += std::string(i ? "," : "") + "cover.c" + std::to_string(i);
  y += "]\ntabs:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  TEST_ASSERT_NOT_NULL(findError(parseConfig(y), "groups.g"));
}

void test_limit_file_size() {
  std::string y = kMinimal;
  y += "# " + std::string(kMaxFileBytes, 'x') + "\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_STRING("", r.errors[0].path.c_str());
}

void test_limit_nesting_depth() {
  std::string y = "version: 1\nx: ";
  for (int i = 0; i < 100; ++i) y += "[";
  for (int i = 0; i < 100; ++i) y += "]";
  y += "\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
}

void test_entity_list_deduplicated_with_group_members() {
  std::string y =
      "version: 1\ngroups:\n  g: [cover.a, cover.b]\n  unused: [light.zzz]\ntabs:\n"
      "  - title: P1\n    widgets:\n      - entity: cover.a\n"
      "      - group: g\n        name: G\n"
      "  - title: P2\n    widgets:\n      - entity: light.x\n      - entity: cover.b\n"
      "      - entity: light.x\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_TRUE(r.ok());
  const auto &ids = r.config.entityIds;
  TEST_ASSERT_EQUAL_UINT(3, ids.size());
  TEST_ASSERT_EQUAL_STRING("cover.a", ids[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cover.b", ids[1].c_str());
  TEST_ASSERT_EQUAL_STRING("light.x", ids[2].c_str());
}

// ---- Hostile input (tester) ----
void test_hostile_billion_laughs() {
  std::string y = "version: 1\na: &a [x,x,x,x,x,x,x,x,x]\n";
  const char *n = "bcdefghi";
  std::string prev = "a";
  for (const char *c = n; *c; ++c) {
    std::string cur(1, *c);
    y += cur + ": &" + cur + " [";
    for (int i = 0; i < 9; ++i) y += std::string(i ? ",*" : "*") + prev;
    y += "]\n";
    prev = cur;
  }
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
}

void test_hostile_duplicate_keys() {
  ParseResult r = parseConfig(std::string("version: 1\nversion: 1\ntabs:\n  - title: P\n"
                                          "    widgets:\n      - entity: light.a\n"));
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_NOT_NULL(findError(r, "version"));
  TEST_ASSERT_TRUE(findError(r, "version")->line == 2);
}

void test_hostile_non_utf8() {
  std::string y = "version: 1\ntabs:\n  - title: \xff\xfe\x80" "bad\n    widgets: []\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
  y = "version: 1\ntabs:\n  - title: \xc3\x28\n    widgets: []\n";
  TEST_ASSERT_FALSE(parseConfig(y).ok());
}

void test_hostile_nul_in_middle() {
  std::string y(kMinimal);
  y.insert(20, 1, '\0');
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
  // NUL via the (const char*, len) overload at various positions.
  for (size_t pos = 0; pos < y.size(); pos += 7) {
    std::string z(kMinimal);
    z.insert(pos, 1, '\0');
    TEST_ASSERT_FALSE(parseConfig(z).ok());
  }
}

void test_hostile_deep_nesting_block_and_flow() {
  std::string y = "version: 1\n";
  for (int i = 0; i < 300; ++i) y += std::string(i * 2, ' ') + "k" + std::to_string(i) + ":\n";
  TEST_ASSERT_FALSE(parseConfig(y).ok());
  std::string f = "version: 1\nx: ";
  for (int i = 0; i < 20000; ++i) f += "[";
  TEST_ASSERT_FALSE(parseConfig(f).ok());
  std::string m = "version: 1\nx: ";
  for (int i = 0; i < 5000; ++i) m += "{a: ";
  TEST_ASSERT_FALSE(parseConfig(m).ok());
}

void test_hostile_huge_scalar() {
  std::string y = "version: 1\ntabs:\n  - title: " + std::string(60000, 'A') +
                  "\n    widgets:\n      - entity: light.a\n";
  ParseResult r = parseConfig(y);  // under 64 KB: accepted or rejected, but no crash
  (void)r;
  std::string z = "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: light." +
                  std::string(60000, 'a') + "\n";
  (void)parseConfig(z);
  std::string many = "version: 1\ngroups:\n";
  for (int i = 0; i < 3000; ++i) many += "  g" + std::to_string(i) + ": [light.a]\n";
  (void)parseConfig(many);
  TEST_PASS();
}

void test_hostile_misc_types() {
  TEST_ASSERT_FALSE(parseConfig(std::string("version: [1]\ntabs: 3\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("? [a, b]\n: c\n")).ok());
  (void)parseConfig(std::string("version: 1\n---\nversion: 1\n"));  // multi-doc: no crash
  TEST_ASSERT_FALSE(parseConfig(std::string("version: !!binary x\ntabs: []\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("version: 1\ntabs:\n  - null\n  - ~\n  - 5\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(nullptr, 5).ok());
  TEST_ASSERT_FALSE(parseConfig("", 0).ok());
}

void test_size_hostile() {
  const char *head = "version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: light.a\n        size: ";
  const char *vals[] = {"\" 2x1\"", "\"2x1 \"", "\"2\xc3\x97" "1\"", "\"-1x2\"", "\"2x1x3\"", "21",
                        "[2, 1]", "{w: 2, h: 1}", "\"\"", "true", "2.5", "\"+2x1\"", "\"2x\\n1\""};
  for (const char *v : vals) {
    ParseResult r = parseConfig(std::string(head) + v + "\n");
    TEST_ASSERT_FALSE_MESSAGE(r.ok(), v);
    TEST_ASSERT_NOT_NULL_MESSAGE(findError(r, "tabs[0].widgets[0].size"), v);
  }
}

void test_group_name_edge_cases() {
  const std::string tail = "tabs:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  ParseResult r = parseConfig("version: 1\ngroups:\n  \"\": [light.a]\n" + tail);
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_NOT_NULL(findError(r, "groups."));
  // Over libyaml's 1024-char simple-key limit: rejected cleanly, no crash.
  (void)parseConfig("version: 1\ngroups:\n  " + std::string(5000, 'a') + ": [light.a]\n" + tail);
  std::string longName(500, 'a');
  r = parseConfig("version: 1\ngroups:\n  " + longName + ": [light.a]\n" + tail);
  TEST_ASSERT_TRUE(r.ok());
  r = parseConfig("version: 1\ngroups:\n  " + longName + "B: [light.a]\n" + tail);
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_NOT_NULL(findError(r, "groups." + longName + "B"));
}


static std::string withWidget(const std::string &extra) {
  return std::string("version: 1\ntabs:\n  - title: P\n    widgets:\n      - entity: sensor.a\n") +
         extra;
}

void test_attr_options_parse() {
  ParseResult r = parseConfig(withWidget("        attribute: humidity\n"
                                         "        details: [dew_point, temperature]\n"
                                         "        unit: \"%\"\n"
                                         "        decimals: 1\n"));
  TEST_ASSERT_TRUE(r.ok());
  const Widget &w = r.config.tabs[0].widgets[0];
  TEST_ASSERT_EQUAL_STRING("humidity", w.attribute.c_str());
  TEST_ASSERT_EQUAL(2, w.details.size());
  TEST_ASSERT_EQUAL_STRING("dew_point", w.details[0].c_str());
  TEST_ASSERT_TRUE(w.unit.has_value());
  TEST_ASSERT_EQUAL_STRING("%", w.unit->c_str());
  TEST_ASSERT_EQUAL(1, *w.decimals);
  auto req = requestedAttributes(r.config);
  TEST_ASSERT_EQUAL(1, req.size());
  TEST_ASSERT_EQUAL_STRING("sensor.a", req[0].first.c_str());
  TEST_ASSERT_EQUAL(3, req[0].second.size());
}

void test_attr_options_absent_by_default() {
  ParseResult r = parseConfig(kMinimal);
  const Widget &w = r.config.tabs[0].widgets[0];
  TEST_ASSERT_TRUE(w.attribute.empty() && w.details.empty() && !w.unit && !w.decimals);
  TEST_ASSERT_TRUE(requestedAttributes(r.config).empty());
}

void test_attr_options_errors() {
  const char *base = "tabs[0].widgets[0].";
  assertErrorAt(withWidget("        attribute: Humidity\n"), std::string(base) + "attribute");
  assertErrorAt(withWidget("        attribute: a-b\n"), std::string(base) + "attribute");
  assertErrorAt(withWidget("        attribute: " + std::string(41, 'a') + "\n"),
                std::string(base) + "attribute");
  assertErrorAt(withWidget("        details: x\n"), std::string(base) + "details");
  assertErrorAt(withWidget("        details: [a, b, c]\n"), std::string(base) + "details");
  assertErrorAt(withWidget("        details: [a, \"B C\"]\n"), std::string(base) + "details[1]");
  assertErrorAt(withWidget("        unit: abcdefghi\n"), std::string(base) + "unit");
  assertErrorAt(withWidget("        decimals: 4\n"), std::string(base) + "decimals");
  assertErrorAt(withWidget("        decimals: x\n"), std::string(base) + "decimals");
  assertErrorAt(withWidget("        decimals: -1\n"), std::string(base) + "decimals");
  // 8 code points with multibyte chars are fine
  TEST_ASSERT_TRUE(parseConfig(withWidget("        unit: \"\xC2\xB0\xC2\xB0\xC2\xB0\xC2\xB0\xC2\xB0\xC2\xB0\xC2\xB0\xC2\xB0\"\n")).ok());
}

void test_attr_errors_do_not_echo_values() {
  ParseResult r = parseConfig(withWidget("        attribute: SecretVal\n        unit: SECRETUNITXX\n"
                                         "        decimals: SECRETDEC\n        details: [SECRETDET]\n"));
  TEST_ASSERT_FALSE(r.ok());
  for (const auto &e : r.errors) TEST_ASSERT_TRUE(e.message.find("SECRET") == std::string::npos &&
                                                  e.message.find("Secret") == std::string::npos);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_example_parses_clean);
  RUN_TEST(test_connection_absent);
  RUN_TEST(test_secrets_parse_ok);
  RUN_TEST(test_secrets_errors_path_line_no_values);
  RUN_TEST(test_secrets_missing_sections_and_top_level);
  RUN_TEST(test_secrets_static_ip_paths);
  RUN_TEST(test_merge_secrets_win);
  RUN_TEST(test_merge_connection_in_config_rejected_with_secrets);
  RUN_TEST(test_merge_secrets_errors_prefixed);
  RUN_TEST(test_merge_without_secrets_keeps_connection_with_warning);
  RUN_TEST(test_merge_config_errors_kept);
  RUN_TEST(test_timezone_default_and_valid);
  RUN_TEST(test_timezone_invalid);
  RUN_TEST(test_error_missing_tabs);
  RUN_TEST(test_pages_key_is_unknown);
  RUN_TEST(test_size_valid);
  RUN_TEST(test_size_invalid);
  RUN_TEST(test_size_hostile);
  RUN_TEST(test_group_name_edge_cases);
  RUN_TEST(test_error_group_name_charset);
  RUN_TEST(test_error_entity_and_group);
  RUN_TEST(test_error_unknown_group);
  RUN_TEST(test_error_group_mixed_domains);
  RUN_TEST(test_error_unsupported_domain);
  RUN_TEST(test_error_external_url_not_https);
  RUN_TEST(test_error_partial_static_ip);
  RUN_TEST(test_error_wrong_version);
  RUN_TEST(test_error_group_widget_needs_name);
  RUN_TEST(test_error_line_number_of_validation);
  RUN_TEST(test_syntax_bad_indentation);
  RUN_TEST(test_syntax_tabs);
  RUN_TEST(test_syntax_truncated);
  RUN_TEST(test_empty_and_garbage);
  RUN_TEST(test_limit_tabs);
  RUN_TEST(test_limit_widgets);
  RUN_TEST(test_limit_group_entities);
  RUN_TEST(test_limit_file_size);
  RUN_TEST(test_limit_nesting_depth);
  RUN_TEST(test_entity_list_deduplicated_with_group_members);
  RUN_TEST(test_hostile_billion_laughs);
  RUN_TEST(test_hostile_duplicate_keys);
  RUN_TEST(test_hostile_non_utf8);
  RUN_TEST(test_hostile_nul_in_middle);
  RUN_TEST(test_hostile_deep_nesting_block_and_flow);
  RUN_TEST(test_hostile_huge_scalar);
  RUN_TEST(test_hostile_misc_types);
  RUN_TEST(test_error_messages_do_not_echo_values);
  RUN_TEST(test_attr_options_parse);
  RUN_TEST(test_attr_options_absent_by_default);
  RUN_TEST(test_attr_options_errors);
  RUN_TEST(test_attr_errors_do_not_echo_values);
  return UNITY_END();
}
