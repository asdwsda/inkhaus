#include <unity.h>

#include "state_codec.h"
#include "state_store.h"

void setUp() {}
void tearDown() {}

static EntityState make(const char* id, const char* state) {
  EntityState e;
  e.id = id;
  e.state = state;
  e.available = true;
  return e;
}

void test_update_reports_new_ids() {
  StateStore s;
  auto ch = s.update({make("light.a", "on"), make("cover.b", "open")});
  TEST_ASSERT_EQUAL(2, ch.size());
  TEST_ASSERT_EQUAL_STRING("light.a", ch[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cover.b", ch[1].c_str());
  TEST_ASSERT_NOT_NULL(s.get("light.a"));
  TEST_ASSERT_NULL(s.get("nope.x"));
}

void test_update_reports_only_changed() {
  StateStore s;
  s.update({make("light.a", "on"), make("cover.b", "open"), make("sensor.c", "1")});
  EntityState c = make("cover.b", "closed");
  auto ch = s.update({make("light.a", "on"), c, make("sensor.c", "1")});
  TEST_ASSERT_EQUAL(1, ch.size());
  TEST_ASSERT_EQUAL_STRING("cover.b", ch[0].c_str());
  TEST_ASSERT_EQUAL_STRING("closed", s.get("cover.b")->state.c_str());
}

void test_update_attribute_and_availability_change() {
  StateStore s;
  EntityState a = make("climate.t", "heat");
  a.temperature = 21.0;
  s.update({a});
  a.temperature = 21.5;
  TEST_ASSERT_EQUAL(1, s.update({a}).size());
  a.available = false;
  TEST_ASSERT_EQUAL(1, s.update({a}).size());
  TEST_ASSERT_EQUAL(0, s.update({a}).size());
}

static EntityState full() {
  EntityState e = make("climate.t", "heat");
  e.last_changed = 1790000000;
  e.friendly_name = "Living";
  e.current_position = 40;
  e.brightness = 128;
  e.battery_level = 77;
  e.current_temperature = 20.5;
  e.temperature = 21.0;
  e.current_humidity = 45.5;
  e.humidity = 50;
  e.min_temp = 7;
  e.max_temp = 30;
  e.target_temp_step = 0.5;
  e.hvac_mode = "heat";
  e.hvac_action = "heating";
  e.unit_of_measurement = "C";
  e.device_class = "temperature";
  e.temperature_unit = "C";
  e.status = "ok";
  e.color_modes = "color_temp,xy";
  e.attrs = {{"a", "1"}, {"b", "x y"}};
  return e;
}

void test_codec_round_trip() {
  std::vector<EntityState> in = {full(), make("light.a", "on")};
  in[1].available = false;
  int64_t ep = 0;
  std::vector<EntityState> out;
  TEST_ASSERT_TRUE(statecodec::unpack(statecodec::pack(in, 1790000123), ep, out));
  TEST_ASSERT_EQUAL(1790000123, (long)ep);
  TEST_ASSERT_TRUE(in == out);
}

void test_codec_color_modes_round_trip() {
  std::vector<EntityState> in = {make("light.a", "on"), make("light.b", "on"), make("light.c", "off")};
  in[1].color_modes = "onoff";
  in[2].color_modes = "color_temp,xy,hs";
  int64_t ep = 0;
  std::vector<EntityState> out;
  TEST_ASSERT_TRUE(statecodec::unpack(statecodec::pack(in, 5), ep, out));
  TEST_ASSERT_EQUAL(3, out.size());
  TEST_ASSERT_EQUAL_STRING("", out[0].color_modes.c_str());
  TEST_ASSERT_EQUAL_STRING("onoff", out[1].color_modes.c_str());
  TEST_ASSERT_EQUAL_STRING("color_temp,xy,hs", out[2].color_modes.c_str());
}

static uint32_t testCrc(const std::string& s) {
  uint32_t c = 0xFFFFFFFFu;
  for (unsigned char b : s) {
    c ^= b;
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

void test_codec_old_format_rejected() {
  // Build a valid format-1 record: no color_modes field (2 zero bytes before the attrs count), valid CRC.
  std::string v2 = statecodec::pack({make("light.a", "on")}, 77);
  std::string body = v2.substr(0, v2.size() - 4);
  body.erase(body.size() - 3, 2);
  body[4] = 1;
  const uint32_t c = testCrc(body);
  for (int i = 0; i < 4; ++i) body.push_back(char((c >> (8 * i)) & 0xFF));
  int64_t ep = 3;
  std::vector<EntityState> out = {make("keep.me", "1")};
  TEST_ASSERT_FALSE(statecodec::unpack(body, ep, out));
  TEST_ASSERT_EQUAL(3, (long)ep);
  TEST_ASSERT_EQUAL(1, out.size());
  TEST_ASSERT_TRUE(statecodec::Source::None == statecodec::restore(body, body, {"light.a"}, ep, out));
}

void test_codec_empty_round_trip() {
  int64_t ep = 5;
  std::vector<EntityState> out = {make("x.y", "1")};
  TEST_ASSERT_TRUE(statecodec::unpack(statecodec::pack({}, 9), ep, out));
  TEST_ASSERT_EQUAL(0, out.size());
}

void test_codec_truncation_and_corruption() {
  const std::string good = statecodec::pack({full(), make("light.a", "on")}, 100);
  int64_t ep = 7;
  std::vector<EntityState> out = {make("keep.me", "1")};
  for (size_t n = 0; n < good.size(); ++n)
    TEST_ASSERT_FALSE(statecodec::unpack(good.substr(0, n), ep, out));
  for (size_t i = 0; i < good.size(); ++i) {
    std::string bad = good;
    bad[i] = char(bad[i] ^ 0x10);
    TEST_ASSERT_FALSE(statecodec::unpack(bad, ep, out));
  }
  TEST_ASSERT_FALSE(statecodec::unpack(good + "x", ep, out));
  TEST_ASSERT_EQUAL(7, (long)ep);
  TEST_ASSERT_EQUAL(1, out.size());
}

void test_restore_prefers_rtc_then_flash_and_filters() {
  const std::string rtc = statecodec::pack({make("light.a", "on")}, 200);
  const std::string fl = statecodec::pack({make("light.a", "off"), make("cover.b", "open")}, 100);
  int64_t ep = 0;
  std::vector<EntityState> out;
  TEST_ASSERT_TRUE(statecodec::Source::Rtc == statecodec::restore(rtc, fl, {"light.a"}, ep, out));
  TEST_ASSERT_EQUAL_STRING("on", out[0].state.c_str());
  TEST_ASSERT_EQUAL(200, (long)ep);
  // bad RTC -> flash; config dropped light.a, kept cover.b
  TEST_ASSERT_TRUE(statecodec::Source::Flash == statecodec::restore("junk", fl, {"cover.b", "new.c"}, ep, out));
  TEST_ASSERT_EQUAL(1, out.size());
  TEST_ASSERT_EQUAL_STRING("cover.b", out[0].id.c_str());
  TEST_ASSERT_EQUAL(100, (long)ep);
  // RTC matches no configured entity -> flash
  TEST_ASSERT_TRUE(statecodec::Source::Flash == statecodec::restore(rtc, fl, {"cover.b"}, ep, out));
  TEST_ASSERT_TRUE(statecodec::Source::None == statecodec::restore("", "bad", {"light.a"}, ep, out));
}

void test_store_all() {
  StateStore s;
  s.update({make("b.x", "1"), make("a.x", "2")});
  auto v = s.all();
  TEST_ASSERT_EQUAL(2, v.size());
  TEST_ASSERT_EQUAL_STRING("a.x", v[0].id.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_update_reports_new_ids);
  RUN_TEST(test_update_reports_only_changed);
  RUN_TEST(test_update_attribute_and_availability_change);
  RUN_TEST(test_codec_round_trip);
  RUN_TEST(test_codec_empty_round_trip);
  RUN_TEST(test_codec_color_modes_round_trip);
  RUN_TEST(test_codec_old_format_rejected);
  RUN_TEST(test_codec_truncation_and_corruption);
  RUN_TEST(test_restore_prefers_rtc_then_flash_and_filters);
  RUN_TEST(test_store_all);
  return UNITY_END();
}
