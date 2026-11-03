#include "device_report.h"

#include <cstdio>
#include <cstdlib>

#include "wall_time.h"

namespace devreport {

const char *wakeKindName(WakeKind k) {
  switch (k) {
    case WakeKind::Scheduled: return "scheduled";
    case WakeKind::Button: return "button";
    case WakeKind::Touch: return "touch";
    default: return "boot";
  }
}

bool sessionReports(bool settingOn, bool scheduledWake, bool coldBoot) {
  return settingOn && (scheduledWake || coldBoot);
}

bool shouldPost(Sensor s, int32_t value, const Record &last, int64_t nowEpoch) {
  if (!last.valid) return true;
  const int32_t delta = s == kBattery ? kBatteryDeltaPct : s == kTemperature ? kTempDeltaX10 : kHumDeltaX10;
  if (std::abs(value - last.value) >= delta) return true;
  // A stored epoch 0 (posted with the clock unset) is stale once the clock is set.
  if (nowEpoch > 0 && (last.epoch <= 0 || nowEpoch - last.epoch > kMaxAgeS)) return true;
  return false;
}

const char *entityId(Sensor s) {
  switch (s) {
    case kBattery: return "sensor.m5paper_battery";
    case kTemperature: return "sensor.m5paper_temperature";
    case kHumidity: return "sensor.m5paper_humidity";
    default: return "sensor.m5paper_last_wake";
  }
}

std::string tenths(int32_t x10) {
  char b[24];
  const int32_t a = std::abs(x10);
  snprintf(b, sizeof b, "%s%d.%d", x10 < 0 ? "-" : "", (int)(a / 10), (int)(a % 10));
  return b;
}

std::string isoUtc(int64_t epoch) {
  const app_logic::Civil c = app_logic::epochToCivil(epoch, 0);
  char b[40];
  snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02d+00:00", c.year, c.month, c.day, c.hour, c.minute, c.second);
  return b;
}

// All strings here are constants or numbers: no JSON escaping needed.
std::string batteryBody(const Readings &r) {
  return "{\"state\":\"" + std::to_string(r.battery_pct) +
         "\",\"attributes\":{\"unit_of_measurement\":\"%\",\"device_class\":\"battery\",\"state_class\":\"measurement\","
         "\"friendly_name\":\"M5Paper battery\",\"voltage_mv\":" + std::to_string(r.voltage_mv) +
         ",\"charging\":" + (r.charging ? "true" : "false") + "}}";
}

std::string temperatureBody(const Readings &r) {
  return "{\"state\":\"" + tenths(r.temp_x10) +
         "\",\"attributes\":{\"unit_of_measurement\":\"\xC2\xB0"
         "C\",\"device_class\":\"temperature\","
         "\"state_class\":\"measurement\",\"friendly_name\":\"M5Paper temperature\"}}";
}

std::string humidityBody(const Readings &r) {
  return "{\"state\":\"" + tenths(r.hum_x10) +
         "\",\"attributes\":{\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\","
         "\"state_class\":\"measurement\",\"friendly_name\":\"M5Paper humidity\"}}";
}

std::string lastWakeBody(int64_t epoch, WakeKind wake) {
  return "{\"state\":\"" + isoUtc(epoch) +
         "\",\"attributes\":{\"device_class\":\"timestamp\",\"friendly_name\":\"M5Paper last wake\",\"reason\":\"" +
         wakeKindName(wake) + "\"}}";
}

std::vector<Item> plan(const Readings &r, const Posted &last, int64_t nowEpoch, bool postLastWake, WakeKind wake) {
  std::vector<Item> v;
  auto add = [&](Sensor s, int32_t value, std::string body) {
    Item it;
    it.sensor = s;
    it.entity = entityId(s);
    it.body = std::move(body);
    it.value = value;
    v.push_back(std::move(it));
  };
  if (shouldPost(kBattery, r.battery_pct, last.rec[kBattery], nowEpoch)) add(kBattery, r.battery_pct, batteryBody(r));
  if (r.have_climate) {
    if (shouldPost(kTemperature, r.temp_x10, last.rec[kTemperature], nowEpoch))
      add(kTemperature, r.temp_x10, temperatureBody(r));
    if (shouldPost(kHumidity, r.hum_x10, last.rec[kHumidity], nowEpoch)) add(kHumidity, r.hum_x10, humidityBody(r));
  }
  if (postLastWake && nowEpoch > 0) add(kLastWake, 0, lastWakeBody(nowEpoch, wake));  // a timestamp needs a clock
  return v;
}

void markPosted(Posted &p, const Item &item, int64_t nowEpoch) {
  Record &r = p.rec[item.sensor];
  r.valid = 1;
  r.value = item.value;
  r.epoch = nowEpoch;
}

}  // namespace devreport
