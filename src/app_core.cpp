// Shared app state (globals), time zone and battery helpers. Split from main.cpp (task 124e).
#include "app_internal.h"

// Task 117: one MEM line (internal heap free / min / largest block, PSRAM free) for stress measurements.
void logMem(const char *tag) {
  Serial.printf("MEM: %s int_free=%u int_min=%u int_largest=%u psram_free=%u\n", tag,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// present::show with the wake log's refresh count and time (task 029; no-op unless the debug log is on).
bool timedShow(present::Mode mode, uint32_t now) {
  const uint32_t t0 = millis();
  const bool did = present::show(mode, now);
  if (did) wakelog::refreshed(millis() - t0);
  return did;
}

uistate::AppState g_app;
render::StatusInfo g_status;
settings::Settings g_settings;

app_config::Loaded g_cfg;

// Task 117: AppState keeps its own copy of the config; the tabs (the bulk: ~0.2 KB per widget of internal heap)
// are dropped from g_cfg after init so a 16-tab dashboard leaves room for Wi-Fi. Use g_app.config() for tabs.
void dropCfgTabs() { std::vector<config::Tab>().swap(g_cfg.cfg.tabs); }
render::SettingsInfo g_info;  // snapshot taken when Settings opens / after a good refresh

// Real mode: the config has a connection section, states come from Home Assistant through the
// session driver. Otherwise the app is the offline demo (fake states, local fake actions).
bool g_real = false;
bool g_demoMode = true;
NetTask g_net;
std::unique_ptr<session_glue::ArduinoWifi> g_wifi;
std::unique_ptr<app_logic::SessionDriver> g_drv;
power::IdleTimer g_localIdle;  // sleep timer when no session runs (demo, config error)
bool g_localIdleOn = false;
uint32_t g_lastInputMs = 0;       // task 095: last user input (touch / key / popup activity)
power::StayAwakeGate g_stayGate;  // feeds the idle timers while staying awake on USB
uint32_t g_usbSampleMs = 0;
bool g_usbSampled = false;
bool g_sessionOver = false;  // the machine asked to sleep; any input starts a new session
bool g_settingsRefreshing = false;
uint32_t g_refreshSeq = 0;  // fetchSeq() when the Settings refresh was issued
bool g_stackLogged = false;
bool g_firstSession = true;
int64_t g_restoredEpoch = 0;  // task 037: when the restored values were last confirmed (0 = none)
power::WakeReason g_wake = power::WakeReason::ColdBoot;  // why this run started (sleepwake::detect)
bool g_scheduled = false;                                // scheduled refresh: no touch/rocker input
app_logic::WakeInputGuard g_wakeGuard;                   // swallows the press that woke the device

// Time zone: POSIX TZ string (config `timezone:`, default CET/CEST). The UTC offset is always
// derived from localtime at the given instant, so DST changes are handled.
void applyTimezone(const std::string &tz) {
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

int32_t utcOffsetAt(int64_t epoch) {
  if (!app_logic::plausibleEpoch(epoch)) return 0;
  const time_t t = time_t(epoch);
  struct tm lt;
  localtime_r(&t, &lt);
  app_logic::Civil c;
  c.year = lt.tm_year + 1900; c.month = lt.tm_mon + 1; c.day = lt.tm_mday;
  c.hour = lt.tm_hour; c.minute = lt.tm_min; c.second = lt.tm_sec;
  return int32_t(app_logic::civilToEpoch(c) - epoch);
}

void appLog(const std::string &line) { Serial.println(line.c_str()); }

int batteryPercent() {
#ifdef TEST_CONFIG_MOCKUP
  return 78;
#else
  if (g_settings.learn_battery && batterylimit::learned(true))  // 0 % = learned limit (task 079)
    return power::battery_percent_scaled(batteryMv(), batteryLimitMv());
  const int v = M5.Power.getBatteryLevel();
  return (v < 0 || v > 100) ? 100 : v;
#endif
}

int32_t batteryMv() {
  const int mv = M5.Power.getBatteryVoltage();
  return mv < 2500 ? 4000 : mv;  // 0 / garbage must not look like an empty battery
}

// Effective low-battery limit (learned or the 3300 default), task 079.
int32_t batteryLimitMv() { return batterylimit::limit(g_settings.learn_battery); }

bool landscape() { return g_app.orientation() == settings::Orientation::Landscape; }
