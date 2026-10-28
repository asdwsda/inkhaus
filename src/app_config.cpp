#include "app_config.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <string>

#include "boot.h"
#include "config_sd.h"

extern const char kDemoYamlStart[] asm("_binary_demo_yaml_start");
extern const char kDemoYamlEnd[] asm("_binary_demo_yaml_end");

namespace app_config {

namespace {

#if defined(TEST_CONFIG_BROKEN)
// Line 7: the entity id has no domain separator; line 9: a widget without entity.
const char kInjected[] =
    "version: 1\n"
    "tabs:\n"
    "  - title: Living room\n"
    "    widgets:\n"
    "      - entity: light.ceiling\n"
    "      - entity: cover.shutter\n"
    "      - entity: notanentity\n"
    "      - name: Nothing\n";
#elif defined(TEST_CONFIG_MOCKUP)
const char kInjected[] =
    "version: 1\n"
    "groups:\n"
    "  downstairs_shutters: [cover.kitchen_shutter, cover.hall_shutter]\n"
    "tabs:\n"
    "  - title: Living room\n"
    "    widgets:\n"
    "      - entity: cover.living_room_shutter\n"
    "        name: Shutter\n"
    "      - group: downstairs_shutters\n"
    "        name: Downstairs shutters\n"
    "      - entity: light.living_room_ceiling\n"
    "      - entity: light.living_room_floor_lamp\n"
    "        name: Floor lamp\n"
    "      - entity: switch.living_room_fan\n"
    "      - entity: climate.living_room\n"
    "      - entity: binary_sensor.living_room_window\n"
    "      - entity: sensor.outside_temperature\n"
    "        name: Outside\n"
    "      - entity: automation.night_mode\n"
    "        name: Night mode\n"
    "      - entity: weather.home\n"
    "  - title: Whole house\n"
    "    widgets:\n"
    "      - entity: vacuum.robot\n"
    "        name: Vacuum\n"
    "      - entity: climate.bedroom\n"
    "      - entity: sensor.outside_temperature\n"
    "        name: Outside temperature today\n"
    "        size: 2x1\n"
    "      - entity: sensor.outside_temperature\n"
    "        name: Outside\n"
    "      - entity: weather.home\n"
    "        size: 1x1\n"
    "      - entity: person.alice\n"
    "        type: sensor\n"
    "  - title: Kitchen\n"
    "    widgets:\n"
    "      - entity: cover.garage_unavailable\n"
    "      - entity: light.garage_unavailable\n"
    "      - entity: switch.garage_unavailable\n"
    "      - entity: climate.garage_unavailable\n"
    "      - entity: sensor.garage_unavailable\n"
    "      - entity: weather.garage_unavailable\n"
    "      - entity: vacuum.garage_unavailable\n"
    "      - entity: binary_sensor.garage_unavailable\n"
    "  - title: Bedroom\n"
    "    widgets:\n"
    "      - entity: switch.pager_0\n"
    "      - entity: switch.pager_1\n"
    "      - entity: switch.pager_2\n"
    "      - entity: switch.pager_3\n"
    "      - entity: switch.pager_4\n"
    "      - entity: switch.pager_5\n"
    "      - entity: switch.pager_6\n"
    "      - entity: switch.pager_7\n"
    "      - entity: switch.pager_8\n"
    "      - entity: switch.pager_9\n"
    "      - entity: switch.pager_10\n"
    "      - entity: switch.pager_11\n"
    "      - entity: switch.pager_12\n"
    "      - entity: switch.pager_13\n"
    "      - entity: switch.pager_14\n"
    "      - entity: switch.pager_15\n"
    "      - entity: switch.pager_16\n"
    "      - entity: switch.pager_17\n"
    "      - entity: switch.pager_18\n"
    "      - entity: switch.pager_19\n"
    "      - entity: switch.pager_20\n"
    "  - title: Garage\n"
    "    widgets:\n"
    "      - entity: switch.garage_door\n";
#endif

Loaded fromParse(config::ParseResult &&pr, bool demo, const char *source) {
  Loaded l;
  l.source = source;
  if (pr.ok()) {
    l.cfg = std::move(pr.config);
    l.demo = demo;
  } else {
    l.error = true;
    l.errors = std::move(pr.errors);
  }
  return l;
}

// Parsed-config cache in LittleFS (task 025): fingerprint (size + mtime of config.yaml and
// secrets.yaml) + binary model. Holds the secrets (merged in); never logged.
constexpr const char *kCachePath = "/config.cache";
constexpr const char *kCacheTmp = "/config.cache.tmp";

bool fsReady() {
  static bool tried = false, ok = false;
  if (!tried) {
    tried = true;
    ok = LittleFS.begin(true);  // formats an unusable partition once
  }
  return ok;
}

bool readCache(const config::Fingerprint &want, config::Config &out) {
  if (!fsReady()) return false;
  File f = LittleFS.open(kCachePath, "r");
  if (!f) return false;
  const size_t n = f.size();
  if (n == 0 || n > 2 * config::kMaxFileBytes) {
    f.close();
    return false;
  }
  std::string data(n, '\0');
  const size_t got = f.read(reinterpret_cast<uint8_t *>(&data[0]), n);
  f.close();
  config::Fingerprint fp;
  return got == n && config::unpackCache(data, fp, out) && fp == want;
}

void writeCache(const config::Fingerprint &fp, const config::Config &cfg) {
  if (!fsReady()) return;
  const std::string data = config::packCache(fp, cfg);
  LittleFS.remove(kCacheTmp);
  File f = LittleFS.open(kCacheTmp, "w");
  if (!f) return;
  const size_t put = f.write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
  f.close();
  if (put == data.size()) LittleFS.remove(kCachePath);
  if (put != data.size() || !LittleFS.rename(kCacheTmp, kCachePath)) {
    LittleFS.remove(kCacheTmp);
    LittleFS.remove(kCachePath);
  }
}

// The demo from demo.yaml (fake values only).
Loaded loadEmbeddedDemo(const char *source) {
  size_t n = size_t(kDemoYamlEnd - kDemoYamlStart);
  while (n > 0 && kDemoYamlStart[n - 1] == '\0') --n;  // embed_txtfiles appends a NUL
  return fromParse(config::parseConfig(kDemoYamlStart, n), true, source);
}

}  // namespace

void invalidateCache() {
  if (fsReady()) {
    LittleFS.remove(kCachePath);
    LittleFS.remove(kCacheTmp);
  }
}

Loaded load(app_logic::ConfigSource source) {
#if defined(TEST_CONFIG_BROKEN) || defined(TEST_CONFIG_MOCKUP)
#if defined(TEST_CONFIG_BROKEN)
  boot::log("config", "test_broken");
  return fromParse(config::parseConfig(kInjected, sizeof(kInjected) - 1), false, "test_broken");
#else
  boot::log("config", "test_mockup");
  return fromParse(config::parseConfig(kInjected, sizeof(kInjected) - 1), true, "test_mockup");
#endif
#else
  if (source == app_logic::ConfigSource::Embedded) {
    Serial.print("CONFIG: source=demo_setting\n");
    return loadEmbeddedDemo("demo_setting");
  }
  const uint32_t t0 = millis();
  config::Fingerprint fp;
  const config_sd::Status st = config_sd::stat(fp);
  if (st == config_sd::Status::Ok) {
    Loaded l;
    if (readCache(fp, l.cfg)) {
      l.source = "cache";
      if (l.cfg.hasConnection && fp.secretsSize == 0)
        Serial.print("CONFIG: warning=connection_in_config\n");
      Serial.printf("CONFIG: source=cache ms=%lu\n", (unsigned long)(millis() - t0));
      return l;
    }
  } else {
    invalidateCache();  // no card / no file: a stale cache must not outlive its source
  }
  config_sd::LoadResult r = config_sd::load();
  boot::log("config_sd", config_sd::statusText(r.status));
  boot::log("config_sd_ms", (long)(millis() - t0));
  switch (r.status) {
    case config_sd::Status::Ok:
      if (r.warnConnectionInConfig) Serial.print("CONFIG: warning=connection_in_config\n");
      if (r.parse.ok() && st == config_sd::Status::Ok) writeCache(fp, r.parse.config);
      else invalidateCache();
      Serial.printf("CONFIG: source=sd ms=%lu\n", (unsigned long)(millis() - t0));
      return fromParse(std::move(r.parse), false, "sd");
    case config_sd::Status::NoCard:
    case config_sd::Status::NoFile: {
      // No config on the card: the demo from demo.yaml (fake values only).
      Serial.printf("CONFIG: source=demo ms=%lu\n", (unsigned long)(millis() - t0));
      return loadEmbeddedDemo("demo");
    }
    default: {
      Loaded l;
      l.source = "sd_error";
      l.error = true;
      l.errors.push_back(config::ConfigError("", config_sd::statusText(r.status), 0));
      return l;
    }
  }
#endif
}

}  // namespace app_config
