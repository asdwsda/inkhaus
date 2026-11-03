#pragma once
// Task 100: what the M5Paper reports to Home Assistant about itself (REST POST /api/states/<entity>).
// Pure logic: the payloads, the "should post" rule and the bookkeeping of what was posted last.
#include <cstdint>
#include <string>
#include <vector>

namespace devreport {

enum Sensor { kBattery = 0, kTemperature = 1, kHumidity = 2, kLastWake = 3, kSensorCount = 4 };

// Thresholds (task 100): a sensor is posted again when it moved this much, or its last post is older than 6 h.
constexpr int32_t kBatteryDeltaPct = 1;
constexpr int32_t kTempDeltaX10 = 2;  // 0.2 C
constexpr int32_t kHumDeltaX10 = 10;  // 1 %
constexpr int64_t kMaxAgeS = 6 * 3600;

struct Readings {
  int32_t battery_pct = 0;
  int32_t voltage_mv = 0;
  bool charging = false;
  bool have_climate = false;  // SHT30 read succeeded (temperature and humidity valid)
  int32_t temp_x10 = 0;
  int32_t hum_x10 = 0;
};

// What was posted last, per sensor. Plain data: the device keeps it in NVS across deep sleep.
struct Record {
  uint8_t valid = 0;
  int32_t value = 0;  // battery %, temperature x10, humidity x10 (unused for last wake)
  int64_t epoch = 0;  // wall clock of the post (0 = unknown clock)
};
struct Posted {
  Record rec[kSensorCount];
};

enum class WakeKind { Scheduled, Button, Touch, Boot };
const char *wakeKindName(WakeKind k);  // scheduled | button | touch | boot

// Does this session report at all? Setting On, and a scheduled wake or the first session after a boot.
bool sessionReports(bool settingOn, bool scheduledWake, bool coldBoot);

// Change / age rule for one measured sensor (battery, temperature, humidity). nowEpoch 0 = clock unset: the age
// rule is skipped then (only a change or a first post triggers).
bool shouldPost(Sensor s, int32_t value, const Record &last, int64_t nowEpoch);

struct Item {
  Sensor sensor = kBattery;
  std::string entity;  // e.g. sensor.m5paper_battery
  std::string body;    // JSON for POST /api/states/<entity>
  int32_t value = 0;
};

// The posts due now: measured sensors by shouldPost (climate only when have_climate), last wake when
// `postLastWake` and the clock is set. Order: battery, temperature, humidity, last wake.
std::vector<Item> plan(const Readings &r, const Posted &last, int64_t nowEpoch, bool postLastWake, WakeKind wake);

// Bookkeeping after a successful post.
void markPosted(Posted &p, const Item &item, int64_t nowEpoch);

// Pieces, exposed for tests.
const char *entityId(Sensor s);
std::string isoUtc(int64_t epoch);  // 2026-11-03T12:00:00+00:00
std::string batteryBody(const Readings &r);
std::string temperatureBody(const Readings &r);
std::string humidityBody(const Readings &r);
std::string lastWakeBody(int64_t epoch, WakeKind wake);
std::string tenths(int32_t x10);  // 215 -> "21.5", -5 -> "-0.5"

}  // namespace devreport
