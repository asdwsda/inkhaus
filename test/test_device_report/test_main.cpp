#include <unity.h>

#include <ArduinoJson.h>
#include "config_model.h"  // keeps the PlatformIO LDF order that resolves the config lib's ArduinoJson dependency (as test_sleep_logic)

#include "device_report.h"
#include "sht30.h"

using namespace devreport;

void setUp() {}
void tearDown() {}

static const int64_t kNow = 1793707200;  // 2026-11-03T12:00:00Z

static Readings readings() {
  Readings r;
  r.battery_pct = 78;
  r.voltage_mv = 3912;
  r.charging = false;
  r.have_climate = true;
  r.temp_x10 = 215;
  r.hum_x10 = 453;
  return r;
}

// ---- SHT30 ----
void test_crc_datasheet_vector() {
  const uint8_t d[2] = {0xBE, 0xEF};
  TEST_ASSERT_EQUAL_HEX8(0x92, sht30::crc8(d, 2));
}

static void frame(uint8_t *raw, uint16_t t, uint16_t h) {
  raw[0] = t >> 8; raw[1] = t & 0xFF; raw[2] = sht30::crc8(raw, 2);
  raw[3] = h >> 8; raw[4] = h & 0xFF; raw[5] = sht30::crc8(raw + 3, 2);
}

void test_convert_values() {
  uint8_t raw[6];
  frame(raw, 0x6666, 0x6666);
  sht30::Reading r = sht30::convert(raw);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT32(250, r.temp_x10);  // 25.0 C
  TEST_ASSERT_EQUAL_INT32(400, r.hum_x10);   // 40.0 %
  frame(raw, 0x0000, 0x0000);
  r = sht30::convert(raw);
  TEST_ASSERT_EQUAL_INT32(-450, r.temp_x10);
  TEST_ASSERT_EQUAL_INT32(0, r.hum_x10);
  frame(raw, 0xFFFF, 0xFFFF);
  r = sht30::convert(raw);
  TEST_ASSERT_EQUAL_INT32(1300, r.temp_x10);
  TEST_ASSERT_EQUAL_INT32(1000, r.hum_x10);
  frame(raw, 0x8000, 0x8000);
  r = sht30::convert(raw);
  TEST_ASSERT_EQUAL_INT32(425, r.temp_x10);
  TEST_ASSERT_EQUAL_INT32(500, r.hum_x10);
}

void test_convert_rejects_bad_crc() {
  uint8_t raw[6];
  frame(raw, 0x6666, 0x6666);
  raw[2] ^= 0x01;
  TEST_ASSERT_FALSE(sht30::convert(raw).ok);
  frame(raw, 0x6666, 0x6666);
  raw[5] ^= 0x80;
  TEST_ASSERT_FALSE(sht30::convert(raw).ok);
  frame(raw, 0x6666, 0x6666);
  raw[1] ^= 0x01;  // data changed, crc stale
  TEST_ASSERT_FALSE(sht30::convert(raw).ok);
}

// ---- payloads ----
static bool parse(const std::string &s, JsonDocument &doc) { return !deserializeJson(doc, s); }

void test_battery_payload() {
  JsonDocument d;
  TEST_ASSERT_TRUE(parse(batteryBody(readings()), d));
  TEST_ASSERT_EQUAL_STRING("78", d["state"]);
  TEST_ASSERT_EQUAL_STRING("%", d["attributes"]["unit_of_measurement"]);
  TEST_ASSERT_EQUAL_STRING("battery", d["attributes"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", d["attributes"]["state_class"]);
  TEST_ASSERT_EQUAL_STRING("M5Paper battery", d["attributes"]["friendly_name"]);
  TEST_ASSERT_EQUAL_INT(3912, d["attributes"]["voltage_mv"].as<int>());
  TEST_ASSERT_TRUE(d["attributes"]["charging"].is<bool>());
  TEST_ASSERT_FALSE(d["attributes"]["charging"].as<bool>());
  Readings c = readings();
  c.charging = true;
  JsonDocument d2;
  TEST_ASSERT_TRUE(parse(batteryBody(c), d2));
  TEST_ASSERT_TRUE(d2["attributes"]["charging"].as<bool>());
}

void test_climate_payloads() {
  JsonDocument d;
  TEST_ASSERT_TRUE(parse(temperatureBody(readings()), d));
  TEST_ASSERT_EQUAL_STRING("21.5", d["state"]);
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0""C", d["attributes"]["unit_of_measurement"]);
  TEST_ASSERT_EQUAL_STRING("temperature", d["attributes"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", d["attributes"]["state_class"]);
  JsonDocument h;
  TEST_ASSERT_TRUE(parse(humidityBody(readings()), h));
  TEST_ASSERT_EQUAL_STRING("45.3", h["state"]);
  TEST_ASSERT_EQUAL_STRING("%", h["attributes"]["unit_of_measurement"]);
  TEST_ASSERT_EQUAL_STRING("humidity", h["attributes"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", h["attributes"]["state_class"]);
  TEST_ASSERT_EQUAL_STRING("M5Paper humidity", h["attributes"]["friendly_name"]);
}

void test_tenths_negative() {
  TEST_ASSERT_EQUAL_STRING("-0.5", tenths(-5).c_str());
  TEST_ASSERT_EQUAL_STRING("-12.3", tenths(-123).c_str());
  TEST_ASSERT_EQUAL_STRING("0.0", tenths(0).c_str());
}

void test_last_wake_payload() {
  TEST_ASSERT_EQUAL_STRING("2026-11-03T12:00:00+00:00", isoUtc(kNow).c_str());
  JsonDocument d;
  TEST_ASSERT_TRUE(parse(lastWakeBody(kNow, WakeKind::Scheduled), d));
  TEST_ASSERT_EQUAL_STRING("2026-11-03T12:00:00+00:00", d["state"]);
  TEST_ASSERT_EQUAL_STRING("timestamp", d["attributes"]["device_class"]);
  TEST_ASSERT_EQUAL_STRING("scheduled", d["attributes"]["reason"]);
  TEST_ASSERT_TRUE(d["attributes"]["unit_of_measurement"].isNull());
  TEST_ASSERT_EQUAL_STRING("boot", wakeKindName(WakeKind::Boot));
  TEST_ASSERT_EQUAL_STRING("button", wakeKindName(WakeKind::Button));
  TEST_ASSERT_EQUAL_STRING("touch", wakeKindName(WakeKind::Touch));
}

void test_entity_ids() {
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_battery", entityId(kBattery));
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_temperature", entityId(kTemperature));
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_humidity", entityId(kHumidity));
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_last_wake", entityId(kLastWake));
}

// ---- rules ----
void test_session_reports() {
  TEST_ASSERT_FALSE(sessionReports(false, true, true));  // setting off: never
  TEST_ASSERT_TRUE(sessionReports(true, true, false));
  TEST_ASSERT_TRUE(sessionReports(true, false, true));
  TEST_ASSERT_FALSE(sessionReports(true, false, false));  // interactive wake
}

void test_should_post_change_and_age() {
  Record none;
  TEST_ASSERT_TRUE(shouldPost(kBattery, 50, none, kNow));  // never posted
  Record r;
  r.valid = 1; r.value = 50; r.epoch = kNow - 600;
  TEST_ASSERT_FALSE(shouldPost(kBattery, 50, r, kNow));
  TEST_ASSERT_TRUE(shouldPost(kBattery, 49, r, kNow));  // >= 1 %
  TEST_ASSERT_TRUE(shouldPost(kBattery, 51, r, kNow));
  r.value = 215;
  TEST_ASSERT_FALSE(shouldPost(kTemperature, 216, r, kNow));
  TEST_ASSERT_TRUE(shouldPost(kTemperature, 217, r, kNow));  // 0.2 C
  TEST_ASSERT_TRUE(shouldPost(kTemperature, 213, r, kNow));
  r.value = 450;
  TEST_ASSERT_FALSE(shouldPost(kHumidity, 459, r, kNow));
  TEST_ASSERT_TRUE(shouldPost(kHumidity, 460, r, kNow));  // 1 %
  // age: exactly 6 h is not yet old, beyond is
  r.value = 50; r.epoch = kNow - 6 * 3600;
  TEST_ASSERT_FALSE(shouldPost(kBattery, 50, r, kNow));
  r.epoch = kNow - 6 * 3600 - 1;
  TEST_ASSERT_TRUE(shouldPost(kBattery, 50, r, kNow));
  // no clock: the age rule is skipped
  TEST_ASSERT_FALSE(shouldPost(kBattery, 50, r, 0));
  r.epoch = 0;
  TEST_ASSERT_FALSE(shouldPost(kBattery, 50, r, kNow));
}

void test_plan_first_time_posts_everything() {
  Posted p;
  const auto v = plan(readings(), p, kNow, true, WakeKind::Scheduled);
  TEST_ASSERT_EQUAL_UINT32(4, v.size());
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_battery", v[0].entity.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_temperature", v[1].entity.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_humidity", v[2].entity.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.m5paper_last_wake", v[3].entity.c_str());
}

void test_plan_skips_unchanged_but_always_last_wake() {
  Posted p;
  for (const Item &it : plan(readings(), p, kNow - 3600, true, WakeKind::Scheduled)) markPosted(p, it, kNow - 3600);
  auto v = plan(readings(), p, kNow, true, WakeKind::Scheduled);
  TEST_ASSERT_EQUAL_UINT32(1, v.size());  // only last wake
  TEST_ASSERT_EQUAL_INT(kLastWake, v[0].sensor);
  Readings r = readings();
  r.battery_pct = 77;
  v = plan(r, p, kNow, false, WakeKind::Boot);
  TEST_ASSERT_EQUAL_UINT32(1, v.size());
  TEST_ASSERT_EQUAL_INT(kBattery, v[0].sensor);
  // old posts (> 6 h) are posted again
  v = plan(readings(), p, kNow + 7 * 3600, false, WakeKind::Boot);
  TEST_ASSERT_EQUAL_UINT32(3, v.size());
}

void test_plan_without_climate_or_clock() {
  Posted p;
  Readings r = readings();
  r.have_climate = false;
  auto v = plan(r, p, kNow, true, WakeKind::Boot);
  TEST_ASSERT_EQUAL_UINT32(2, v.size());  // battery + last wake
  v = plan(r, p, 0, true, WakeKind::Boot);  // clock unset: no timestamp sensor
  TEST_ASSERT_EQUAL_UINT32(1, v.size());
  TEST_ASSERT_EQUAL_INT(kBattery, v[0].sensor);
}

void test_mark_posted() {
  Posted p;
  Item it;
  it.sensor = kTemperature;
  it.value = 215;
  markPosted(p, it, kNow);
  TEST_ASSERT_TRUE(p.rec[kTemperature].valid);
  TEST_ASSERT_EQUAL_INT32(215, p.rec[kTemperature].value);
  TEST_ASSERT_EQUAL_INT64(kNow, p.rec[kTemperature].epoch);
  TEST_ASSERT_FALSE(p.rec[kBattery].valid);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc_datasheet_vector);
  RUN_TEST(test_convert_values);
  RUN_TEST(test_convert_rejects_bad_crc);
  RUN_TEST(test_battery_payload);
  RUN_TEST(test_climate_payloads);
  RUN_TEST(test_tenths_negative);
  RUN_TEST(test_last_wake_payload);
  RUN_TEST(test_entity_ids);
  RUN_TEST(test_session_reports);
  RUN_TEST(test_should_post_change_and_age);
  RUN_TEST(test_plan_first_time_posts_everything);
  RUN_TEST(test_plan_skips_unchanged_but_always_last_wake);
  RUN_TEST(test_plan_without_climate_or_clock);
  RUN_TEST(test_mark_posted);
  return UNITY_END();
}
