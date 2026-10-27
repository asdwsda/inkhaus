#include <unity.h>

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
    "pages:\n"
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
  TEST_ASSERT_TRUE(c.hasConnection);
  TEST_ASSERT_EQUAL_STRING("MyWifi", c.connection.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secret", c.connection.wifi.password.c_str());
  TEST_ASSERT_TRUE(c.connection.wifi.hasStaticIp);
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", c.connection.wifi.staticIp.ip.c_str());
  TEST_ASSERT_EQUAL_STRING("255.255.255.0", c.connection.wifi.staticIp.subnet.c_str());
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.10:8123", c.connection.ha.internalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("https://example.ui.nabu.casa", c.connection.ha.externalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("long-lived-token", c.connection.ha.token.c_str());

  TEST_ASSERT_EQUAL_UINT(1, c.groups.size());
  TEST_ASSERT_EQUAL_STRING("downstairs_shutters", c.groups[0].name.c_str());
  TEST_ASSERT_EQUAL_UINT(2, c.groups[0].members.size());

  TEST_ASSERT_EQUAL_UINT(1, c.pages.size());
  TEST_ASSERT_EQUAL_STRING("Living room", c.pages[0].title.c_str());
  const auto &w = c.pages[0].widgets;
  TEST_ASSERT_EQUAL_UINT(5, w.size());
  TEST_ASSERT_TRUE(w[0].type == WidgetType::Light);
  TEST_ASSERT_EQUAL_STRING("Ceiling", w[0].name.c_str());
  TEST_ASSERT_TRUE(w[1].type == WidgetType::Cover);
  TEST_ASSERT_EQUAL_STRING("downstairs_shutters", w[1].group.c_str());
  TEST_ASSERT_EQUAL_UINT(2, w[1].entities.size());
  TEST_ASSERT_TRUE(w[2].type == WidgetType::Climate);
  TEST_ASSERT_EQUAL_STRING("", w[2].name.c_str());
  TEST_ASSERT_TRUE(w[3].type == WidgetType::Sensor);
}

void test_connection_absent() {
  ParseResult r = parseConfig(kMinimal);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_FALSE(r.config.hasConnection);
  TEST_ASSERT_EQUAL_UINT(1, r.config.pages.size());
}

void test_error_missing_pages() { assertErrorAt("version: 1\n", "pages"); }

void test_error_entity_and_group() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a]\npages:\n  - title: P\n"
                            "    widgets:\n      - entity: light.a\n        group: g\n"
                            "        name: n\n"),
                "pages[0].widgets[0]");
}

void test_error_unknown_group() {
  assertErrorAt(std::string("version: 1\npages:\n  - title: P\n    widgets:\n"
                            "      - group: nope\n        name: n\n"),
                "pages[0].widgets[0].group");
}

void test_error_group_mixed_domains() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a, cover.b]\npages:\n"
                            "  - title: P\n    widgets:\n      - group: g\n        name: n\n"),
                "groups.g");
}

void test_error_unsupported_domain() {
  assertErrorAt(std::string("version: 1\npages:\n  - title: P\n    widgets:\n"
                            "      - entity: person.bob\n"),
                "pages[0].widgets[0].entity");
  // With explicit type it is accepted.
  ParseResult r = parseConfig(std::string("version: 1\npages:\n  - title: P\n    widgets:\n"
                                          "      - entity: person.bob\n        type: sensor\n"));
  TEST_ASSERT_TRUE(r.ok());
}

void test_error_external_url_not_https() {
  std::string y =
      "version: 1\nconnection:\n  wifi:\n    ssid: a\n  home_assistant:\n"
      "    internal_url: http://x:8123\n    external_url: http://y\n    token: t\n"
      "pages:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  assertErrorAt(y, "connection.home_assistant.external_url");
}

void test_error_partial_static_ip() {
  std::string y =
      "version: 1\nconnection:\n  wifi:\n    ssid: a\n    static_ip:\n      ip: 192.168.1.5\n"
      "      gateway: 192.168.1.1\n  home_assistant:\n    internal_url: http://x\n    token: t\n"
      "pages:\n  - title: P\n    widgets:\n      - entity: light.a\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_NOT_NULL(findError(r, "connection.wifi.static_ip.subnet"));
  TEST_ASSERT_NOT_NULL(findError(r, "connection.wifi.static_ip.dns"));
  TEST_ASSERT_TRUE(findError(r, "connection.wifi.static_ip.dns")->line > 0);
}

void test_error_wrong_version() {
  ParseResult r = parseConfig(std::string("version: 2\npages:\n  - title: P\n    widgets: []\n"));
  const ConfigError *e = findError(r, "version");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(1, e->line);
}

void test_error_group_widget_needs_name() {
  assertErrorAt(std::string("version: 1\ngroups:\n  g: [light.a]\npages:\n  - title: P\n"
                            "    widgets:\n      - group: g\n"),
                "pages[0].widgets[0].name");
}

void test_error_line_number_of_validation() {
  ParseResult r = parseConfig(std::string("version: 1\npages:\n  - title: P\n    widgets:\n"
                                          "      - entity: light.a\n      - entity: person.x\n"));
  const ConfigError *e = findError(r, "pages[0].widgets[1].entity");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(6, e->line);
}

void test_syntax_bad_indentation() {
  assertSyntaxError("version: 1\npages:\n  - title: P\n   widgets: []\n");
}

void test_syntax_tabs() {
  ParseResult r = parseConfig(std::string("version: 1\npages:\n\t- title: P\n"));
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_INT(3, r.errors[0].line);
}

void test_syntax_truncated() {
  assertSyntaxError("version: 1\npages:\n  - title: \"Living ro");
  assertSyntaxError("version: 1\ngroups:\n  g: [light.a, light.b\n");
}

void test_empty_and_garbage() {
  TEST_ASSERT_FALSE(parseConfig(std::string("")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("just a string")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("- a\n- b\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("a: &x 1\nb: *x\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("\x01\xff\xfe: [\n")).ok());
}

void test_limit_pages() {
  std::string y = "version: 1\npages:\n";
  for (int i = 0; i < 17; ++i) y += "  - title: P\n    widgets:\n      - entity: light.a\n";
  TEST_ASSERT_NOT_NULL(findError(parseConfig(y), "pages"));
  y = "version: 1\npages:\n";
  for (int i = 0; i < 16; ++i) y += "  - title: P\n    widgets:\n      - entity: light.a\n";
  TEST_ASSERT_TRUE(parseConfig(y).ok());
}

void test_limit_widgets() {
  std::string head = "version: 1\npages:\n  - title: P\n    widgets:\n";
  std::string y = head;
  for (int i = 0; i < 25; ++i) y += "      - entity: light.a" + std::to_string(i) + "\n";
  TEST_ASSERT_NOT_NULL(findError(parseConfig(y), "pages[0].widgets"));
  y = head;
  for (int i = 0; i < 24; ++i) y += "      - entity: light.a" + std::to_string(i) + "\n";
  TEST_ASSERT_TRUE(parseConfig(y).ok());
}

void test_limit_group_entities() {
  std::string y = "version: 1\ngroups:\n  g: [";
  for (int i = 0; i < 17; ++i) y += std::string(i ? "," : "") + "cover.c" + std::to_string(i);
  y += "]\npages:\n  - title: P\n    widgets:\n      - entity: light.a\n";
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
      "version: 1\ngroups:\n  g: [cover.a, cover.b]\n  unused: [light.zzz]\npages:\n"
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
  ParseResult r = parseConfig(std::string("version: 1\nversion: 1\npages:\n  - title: P\n"
                                          "    widgets:\n      - entity: light.a\n"));
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_NOT_NULL(findError(r, "version"));
  TEST_ASSERT_TRUE(findError(r, "version")->line == 2);
}

void test_hostile_non_utf8() {
  std::string y = "version: 1\npages:\n  - title: \xff\xfe\x80" "bad\n    widgets: []\n";
  ParseResult r = parseConfig(y);
  TEST_ASSERT_FALSE(r.ok());
  y = "version: 1\npages:\n  - title: \xc3\x28\n    widgets: []\n";
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
  std::string y = "version: 1\npages:\n  - title: " + std::string(60000, 'A') +
                  "\n    widgets:\n      - entity: light.a\n";
  ParseResult r = parseConfig(y);  // under 64 KB: accepted or rejected, but no crash
  (void)r;
  std::string z = "version: 1\npages:\n  - title: P\n    widgets:\n      - entity: light." +
                  std::string(60000, 'a') + "\n";
  (void)parseConfig(z);
  std::string many = "version: 1\ngroups:\n";
  for (int i = 0; i < 3000; ++i) many += "  g" + std::to_string(i) + ": [light.a]\n";
  (void)parseConfig(many);
  TEST_PASS();
}

void test_hostile_misc_types() {
  TEST_ASSERT_FALSE(parseConfig(std::string("version: [1]\npages: 3\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("? [a, b]\n: c\n")).ok());
  (void)parseConfig(std::string("version: 1\n---\nversion: 1\n"));  // multi-doc: no crash
  TEST_ASSERT_FALSE(parseConfig(std::string("version: !!binary x\npages: []\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(std::string("version: 1\npages:\n  - null\n  - ~\n  - 5\n")).ok());
  TEST_ASSERT_FALSE(parseConfig(nullptr, 5).ok());
  TEST_ASSERT_FALSE(parseConfig("", 0).ok());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_example_parses_clean);
  RUN_TEST(test_connection_absent);
  RUN_TEST(test_error_missing_pages);
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
  RUN_TEST(test_limit_pages);
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
  return UNITY_END();
}
