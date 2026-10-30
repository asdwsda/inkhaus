#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "config_cache.h"

using namespace config;

void setUp() {}
void tearDown() {}

namespace {

Config fullConfig() {
  Config c;
  c.version = 1;
  c.timezone = "UTC0";
  c.hasConnection = true;
  c.connection.wifi = {"ssid-x", "pw-y", true, {"10.0.0.2", "10.0.0.1", "255.255.255.0", "1.1.1.1"}, "Home",
                       HaUrlMode::Internal};
  WifiConfig second;
  second.ssid = "phone";
  second.name = "Hotspot";
  second.haUrl = HaUrlMode::External;
  c.connection.wifiProfiles = {c.connection.wifi, second};
  c.connection.ha = {"http://a:8123", "https://b", "tok"};
  c.groups = {{"g1", {"cover.a", "cover.b"}}, {"g2", {}}};
  Widget w1;
  w1.type = WidgetType::Cover;
  w1.group = "g1";
  w1.name = "Shutters";
  w1.size = WidgetSize{2, 3};
  w1.entities = {"cover.a", "cover.b"};
  Widget w2;
  w2.type = WidgetType::Vacuum;
  w2.entity = "vacuum.r";
  w2.entities = {"vacuum.r"};
  c.tabs = {{"One", {w1, w2}}, {"Empty", {}}};
  c.entityIds = {"cover.a", "cover.b", "vacuum.r"};
  return c;
}

void expectEqual(const Config &a, const Config &b) {
  // Re-serialising is deterministic, so equal blobs mean equal field by field.
  TEST_ASSERT_TRUE(serializeConfig(a) == serializeConfig(b));
  TEST_ASSERT_EQUAL_INT(a.version, b.version);
  TEST_ASSERT_EQUAL_STRING(a.timezone.c_str(), b.timezone.c_str());
  TEST_ASSERT_EQUAL(a.hasConnection, b.hasConnection);
  TEST_ASSERT_EQUAL_STRING(a.connection.wifi.staticIp.dns.c_str(), b.connection.wifi.staticIp.dns.c_str());
  TEST_ASSERT_EQUAL(a.connection.wifiProfiles.size(), b.connection.wifiProfiles.size());
  for (size_t i = 0; i < a.connection.wifiProfiles.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(a.connection.wifiProfiles[i].name.c_str(), b.connection.wifiProfiles[i].name.c_str());
    TEST_ASSERT_TRUE(a.connection.wifiProfiles[i].haUrl == b.connection.wifiProfiles[i].haUrl);
  }
  TEST_ASSERT_EQUAL_STRING(a.connection.ha.token.c_str(), b.connection.ha.token.c_str());
  TEST_ASSERT_EQUAL(a.tabs.size(), b.tabs.size());
  TEST_ASSERT_EQUAL(a.groups.size(), b.groups.size());
}

}  // namespace

void test_roundtrip_all_fields() {
  Config c = fullConfig(), out;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  expectEqual(c, out);
  TEST_ASSERT_EQUAL_STRING("UTC0", out.timezone.c_str());
  TEST_ASSERT_TRUE(out.connection.wifi.hasStaticIp);
  TEST_ASSERT_EQUAL_STRING("10.0.0.1", out.connection.wifi.staticIp.gateway.c_str());
  TEST_ASSERT_EQUAL_STRING("https://b", out.connection.ha.externalUrl.c_str());
  TEST_ASSERT_EQUAL_STRING("g1", out.groups[0].name.c_str());
  TEST_ASSERT_EQUAL(2, out.groups[0].members.size());
  TEST_ASSERT_TRUE(out.tabs[0].widgets[0].size.has_value());
  TEST_ASSERT_EQUAL(2, out.tabs[0].widgets[0].size->w);
  TEST_ASSERT_EQUAL(3, out.tabs[0].widgets[0].size->h);
  TEST_ASSERT_FALSE(out.tabs[0].widgets[1].size.has_value());
  TEST_ASSERT_TRUE(out.tabs[0].widgets[1].type == WidgetType::Vacuum);
  TEST_ASSERT_EQUAL(3, out.entityIds.size());
}

void test_roundtrip_example_yaml() {
  std::ifstream f("config.example.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  ParseResult pr = parseConfig(ss.str());
  TEST_ASSERT_TRUE(pr.ok());
  Config out;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(pr.config), out));
  expectEqual(pr.config, out);
}

void test_roundtrip_default_config() {
  Config c, out;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  expectEqual(c, out);
}

void test_truncated_rejected() {
  std::string s = serializeConfig(fullConfig());
  Config out;
  for (size_t n = 0; n < s.size(); ++n) TEST_ASSERT_FALSE(deserializeConfig(s.substr(0, n), out));
}

void test_bitflip_rejected() {
  std::string s = serializeConfig(fullConfig());
  Config out;
  for (size_t i = 0; i < s.size(); ++i) {
    std::string t = s;
    t[i] ^= 0x01;
    TEST_ASSERT_FALSE(deserializeConfig(t, out));
  }
}

void test_wrong_format_and_garbage_rejected() {
  Config out;
  TEST_ASSERT_FALSE(deserializeConfig(std::string("version: 1\n"), out));
  TEST_ASSERT_FALSE(deserializeConfig(std::string(), out));
  TEST_ASSERT_FALSE(deserializeConfig(nullptr, 100, out));
}

void test_pack_unpack_fingerprint() {
  Config c = fullConfig(), out;
  Fingerprint fp{1234, 99999}, got;
  std::string f = packCache(fp, c);
  TEST_ASSERT_TRUE(unpackCache(f, got, out));
  TEST_ASSERT_TRUE(got == fp);
  expectEqual(c, out);
  TEST_ASSERT_FALSE(got == (Fingerprint{1234, 1}));
  // The fingerprint covers secrets.yaml too.
  Fingerprint fp2{1234, 99999, 77, 5}, got2;
  std::string f2 = packCache(fp2, c);
  TEST_ASSERT_TRUE(unpackCache(f2, got2, out));
  TEST_ASSERT_TRUE(got2 == fp2);
  TEST_ASSERT_FALSE(got2 == fp);
  TEST_ASSERT_FALSE(got2 == (Fingerprint{1234, 99999, 77, 6}));
  TEST_ASSERT_FALSE(got2 == (Fingerprint{1234, 99999, 78, 5}));
  f[2] ^= 1;  // fingerprint bytes are not covered by the blob CRC: a mismatch is just a cache miss
  TEST_ASSERT_TRUE(unpackCache(f, got, out));
  TEST_ASSERT_FALSE(got == fp);
  TEST_ASSERT_FALSE(unpackCache(f.substr(0, f.size() - 1), got, out));
  TEST_ASSERT_FALSE(unpackCache("short", got, out));
}


void test_more_than_four_profiles_rejected() {
  Config c = fullConfig(), out;
  c.connection.wifiProfiles.assign(5, c.connection.wifi);
  TEST_ASSERT_FALSE(deserializeConfig(serializeConfig(c), out));
  c.connection.wifiProfiles.resize(4);
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  TEST_ASSERT_EQUAL(4, (int)out.connection.wifiProfiles.size());
}

void test_roundtrip_landscape_columns() {
  Config c = fullConfig(), out;
  c.tabs[0].landscapeColumns = 4;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  TEST_ASSERT_EQUAL_INT(4, out.tabs[0].landscapeColumns);
  TEST_ASSERT_EQUAL_INT(5, out.tabs[1].landscapeColumns);
}

void test_roundtrip_attribute_options() {
  Config c = fullConfig(), out;
  Widget &w = c.tabs[0].widgets[1];
  w.attribute = "humidity";
  w.details = {"dew_point", "temperature"};
  w.unit = "%";
  w.decimals = 2;
  c.tabs[0].widgets[0].unit = "";  // present but empty stays present
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  const Widget &o = out.tabs[0].widgets[1];
  TEST_ASSERT_EQUAL_STRING("humidity", o.attribute.c_str());
  TEST_ASSERT_EQUAL(2, o.details.size());
  TEST_ASSERT_EQUAL_STRING("temperature", o.details[1].c_str());
  TEST_ASSERT_TRUE(o.unit.has_value());
  TEST_ASSERT_EQUAL_STRING("%", o.unit->c_str());
  TEST_ASSERT_EQUAL(2, *o.decimals);
  TEST_ASSERT_TRUE(out.tabs[0].widgets[0].unit.has_value());
  TEST_ASSERT_FALSE(out.tabs[0].widgets[0].decimals.has_value());
  expectEqual(c, out);
}

void test_roundtrip_history() {
  Config c = fullConfig(), out;
  HistoryConfig &h = c.tabs[0].widgets[1].history;
  h.hours = 168;
  h.type = HistoryType::Bar;
  h.statistic = HistoryStatistic::Change;
  h.period = HistoryPeriod::Day;
  h.min = -2.5f;
  h.max = 30.f;
  h.source = HistorySource::Statistics;
  c.tabs[0].widgets[0].history.enabled = false;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  const HistoryConfig &o = out.tabs[0].widgets[1].history;
  TEST_ASSERT_EQUAL(168, o.hours);
  TEST_ASSERT_TRUE(o.type == HistoryType::Bar && o.period == HistoryPeriod::Day &&
                   o.source == HistorySource::Statistics);
  TEST_ASSERT_TRUE(o.statistic && *o.statistic == HistoryStatistic::Change);
  TEST_ASSERT_EQUAL_FLOAT(-2.5f, *o.min);
  TEST_ASSERT_EQUAL_FLOAT(30.f, *o.max);
  TEST_ASSERT_FALSE(out.tabs[0].widgets[0].history.enabled);
  TEST_ASSERT_FALSE(out.tabs[0].widgets[0].history.min.has_value());
  expectEqual(c, out);
}

void test_roundtrip_display() {
  Config c = fullConfig(), out;
  c.tabs[0].widgets[1].display = WidgetDisplay::Graph;
  TEST_ASSERT_TRUE(deserializeConfig(serializeConfig(c), out));
  TEST_ASSERT_TRUE(out.tabs[0].widgets[1].display == WidgetDisplay::Graph);
  TEST_ASSERT_TRUE(out.tabs[0].widgets[0].display == WidgetDisplay::Default);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_all_fields);
  RUN_TEST(test_roundtrip_display);
  RUN_TEST(test_more_than_four_profiles_rejected);
  RUN_TEST(test_roundtrip_landscape_columns);
  RUN_TEST(test_roundtrip_example_yaml);
  RUN_TEST(test_roundtrip_default_config);
  RUN_TEST(test_truncated_rejected);
  RUN_TEST(test_bitflip_rejected);
  RUN_TEST(test_wrong_format_and_garbage_rejected);
  RUN_TEST(test_pack_unpack_fingerprint);
  RUN_TEST(test_roundtrip_attribute_options);
  RUN_TEST(test_roundtrip_history);
  return UNITY_END();
}
