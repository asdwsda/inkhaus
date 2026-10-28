#include <M5Unified.h>

#include <time.h>

#include <map>
#include <memory>
#include <vector>

#include "app_config.h"
#include "app_state.h"
#include "boot.h"
#include "input.h"
#include "net_task.h"
#include "present.h"
#include "render/screen.h"
#include "render/settings_screen.h"
#include "session_driver.h"
#include "session_glue.h"
#include "settings.h"
#include "settings_flow.h"
#include "settings_nvs.h"
#include "shot.h"
#include "tap_intent.h"
#include "wall_time.h"

#ifdef TLS_CHECK
namespace tls_check { void run(); }
#endif

// Test-only build flags (never in the normal firmware):
//   -DDEMO_LANDSCAPE       start in landscape instead of the stored orientation
//   -DTEST_CONFIG_BROKEN   inject a broken config (error screen)
//   -DTEST_CONFIG_MOCKUP   inject the approved mockup's config, mock status label, frozen pending
//   -DBOOT_TEST_HANG=n / -DBOOT_TEST_RESTART  task 016 watchdog and warm-restart tests
//   -DTEST_SETTINGS       open the Settings screen right after boot (screenshots, task 022/023)
//   -DTLS_CHECK -DTLS_CHECK_URL=\"https://...\"  task 008 TLS check after boot (src/tls_check.cpp)

static uistate::AppState g_app;
static render::StatusInfo g_status;
static settings::Settings g_settings;
static app_config::Loaded g_cfg;
static render::SettingsInfo g_info;  // snapshot taken when Settings opens / after a good refresh

// Real mode: the config has a connection section, states come from Home Assistant through the
// session driver. Otherwise the app is the offline demo (fake states, local fake actions).
static bool g_real = false;
static bool g_demoMode = true;
static NetTask g_net;
static std::unique_ptr<session_glue::ArduinoWifi> g_wifi;
static std::unique_ptr<app_logic::SessionDriver> g_drv;
static bool g_sessionOver = false;  // the machine asked to sleep; any input starts a new session
static bool g_settingsRefreshing = false;
static uint32_t g_refreshSeq = 0;  // fetchSeq() when the Settings refresh was issued
static bool g_stackLogged = false;
static bool g_firstSession = true;

// Time zone: POSIX TZ string (config `timezone:`, default CET/CEST). The UTC offset is always
// derived from localtime at the given instant, so DST changes are handled.
static void applyTimezone(const std::string &tz) {
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

static int32_t utcOffsetAt(int64_t epoch) {
  if (!app_logic::plausibleEpoch(epoch)) return 0;
  const time_t t = time_t(epoch);
  struct tm lt;
  localtime_r(&t, &lt);
  app_logic::Civil c;
  c.year = lt.tm_year + 1900; c.month = lt.tm_mon + 1; c.day = lt.tm_mday;
  c.hour = lt.tm_hour; c.minute = lt.tm_min; c.second = lt.tm_sec;
  return int32_t(app_logic::civilToEpoch(c) - epoch);
}

static void appLog(const std::string &line) { Serial.println(line.c_str()); }

static int batteryPercent() {
#ifdef TEST_CONFIG_MOCKUP
  return 78;
#else
  const int v = M5.Power.getBatteryLevel();
  return (v < 0 || v > 100) ? 100 : v;
#endif
}

static int32_t batteryMv() {
  const int mv = M5.Power.getBatteryVoltage();
  return mv < 2500 ? 4000 : mv;  // 0 / garbage must not look like an empty battery
}

static bool landscape() { return g_app.orientation() == settings::Orientation::Landscape; }

static void updateStatus() {
  g_status.batteryPct = batteryPercent();
  if (!g_real || !g_drv) return;
  using L = render::Link;
  switch (g_drv->status()) {
    case session::Status::Connecting: g_status.link = L::Connecting; break;
    case session::Status::Online: g_status.link = g_drv->fetching() ? L::Connecting : L::Online; break;
    case session::Status::Offline: g_status.link = L::Offline; break;
    case session::Status::AuthError: g_status.link = L::AuthError; break;
  }
  const int64_t nowEpoch = session_glue::wallClock();
  const app_logic::Label l = app_logic::refreshLabel(g_drv->lastOkEpoch(), nowEpoch, utcOffsetAt(nowEpoch),
                                                     g_settings.refresh_interval_min, g_drv->lastFetchFailed(),
                                                     landscape());
  g_status.label1 = l.line1;
  g_status.label2 = l.line2;
  g_status.stale = l.stale;
}

static void composeFrame() {
  updateStatus();
  render::renderScreen(present::canvas(), g_app, g_status);
}

static settings_ui::WallTime wallTime(int64_t epoch) {
  settings_ui::WallTime w;
  if (!app_logic::plausibleEpoch(epoch)) return w;
  const app_logic::Civil c = app_logic::epochToCivil(epoch, utcOffsetAt(epoch));
  w.month = c.month; w.day = c.day; w.hour = c.hour; w.minute = c.minute;
  return w;
}

static void snapshotInfo() {
  render::SettingsInfo &i = g_info;
  i.firmware = "v0.23";
  i.battery.percent = batteryPercent();
  i.battery.millivolts = batteryMv();
  i.battery.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  i.wifi.configured = g_real;
  i.wifi.ssid = g_cfg.cfg.connection.wifi.ssid;
  i.wifi.connected = g_drv && g_drv->wifiUp();
  i.config.missing = g_cfg.demo;
  i.config.errorCount = int(g_cfg.errors.size());
  if (!g_cfg.errors.empty()) {
    i.config.firstPath = g_cfg.errors[0].path;
    i.config.firstLine = g_cfg.errors[0].line;
  }
  i.now = wallTime(session_glue::wallClock());
  settings_ui::HaInfo &h = i.ha;
  h = settings_ui::HaInfo();
  if (g_drv) {
    h.sessionDone = g_drv->sessionDone();
    h.lastAttemptOk = !g_drv->lastFetchFailed();
    h.everOk = g_drv->lastOkEpoch() > 0;
    h.lastOk = wallTime(g_drv->lastOkEpoch());
    h.lastLink = !h.everOk ? settings_ui::HaInfo::Link::None
                 : g_drv->lastLinkExternal() ? settings_ui::HaInfo::Link::Remote
                                             : settings_ui::HaInfo::Link::Local;
    h.refreshing = g_settingsRefreshing;
  }
}

static void drawSettingsFrame(present::Mode mode, uint32_t now) {
  render::drawSettings(present::canvas(), settings_flow::screen(), g_info);
  present::show(mode, now);
}

static void showFrame(uint32_t now) {
  if (settings_flow::active()) {
    drawSettingsFrame(present::Mode::Partial, now);
    return;
  }
  composeFrame();
  const bool full = g_app.takeFull();
  present::show(full ? present::Mode::Full : present::Mode::Partial, now);
}

// ---- session ----

static void startSession(uint32_t now) {
  g_net.reset();
  session_glue::ntpSessionStart();
  g_drv.reset();  // before the Wi-Fi adapter it points to
  g_wifi.reset(new session_glue::ArduinoWifi(g_cfg.cfg.connection.wifi));
  g_drv.reset(new app_logic::SessionDriver(g_cfg.cfg.entityIds, g_app.store(), *g_wifi, g_net,
                                           session_glue::wallClock, appLog));
  g_sessionOver = false;
  // The offset is taken now (local time at planning time); the machine plans the wake-up from it.
  g_drv->start(g_firstSession ? power::WakeReason::ColdBoot : power::WakeReason::Button, batteryMv(), g_settings,
               utcOffsetAt(session_glue::wallClock()), now);
  g_firstSession = false;
}

// Action sink: a tap started an action in the UI state; real mode turns it into a session action.
static uint32_t g_tapNow = 0;
static void onAppAction(int tab, int widget, int sub, const std::vector<EntityState> &requested) {
  if (!g_real || !g_drv) return;
  const auto &tabs = g_app.config().tabs;
  if (tab < 0 || size_t(tab) >= tabs.size() || widget < 0 || size_t(widget) >= tabs[size_t(tab)].widgets.size())
    return;
  const config::Widget &w = tabs[size_t(tab)].widgets[size_t(widget)];
  auto ev = app_logic::intentFromRequest(w.type, sub, requested);
  if (ev) g_drv->userAction(*ev, g_tapNow);
}

// Driver marks (pending/failed by entity id) -> widget visuals. True if something visible changed.
static bool syncMarks() {
  if (!g_real || !g_drv) return false;
  std::map<std::string, uistate::Mark> m;
  for (const auto &kv : g_drv->marks())
    m[kv.first] = kv.second == session::Mark::Failed ? uistate::Mark::Failed : uistate::Mark::Pending;
  return g_app.setMarks(m);
}

// ---- settings screen ----

static void applyOrientation(settings::Orientation o) {
  std::vector<EntityState> keep;
  for (const std::string &id : g_cfg.cfg.entityIds)
    if (const EntityState *s = g_app.store().get(id)) keep.push_back(*s);
  M5.Display.setRotation(o == settings::Orientation::Landscape ? 1 : 0);
  present::begin();
  g_app.init(g_cfg.cfg, o, g_demoMode);
  g_app.setActionSink(onAppAction);
  g_app.setMarksDriven(g_real);
  if (!g_demoMode) g_app.store().update(keep);
  boot::log("orientation", (long)(o == settings::Orientation::Landscape ? 1 : 0));
}

static void closeSettings(const settings_ui::LeaveResult &r, uint32_t now) {
  g_settingsRefreshing = false;
  if (r.save) {
    NvsSettingsStore store;
    if (store.begin()) {
      settings::save(store, r.settings);
      store.end();
    }
    Serial.println("APP: settings saved");
  }
  g_settings = r.settings;
  if (r.orientationChanged) {
    applyOrientation(r.settings.orientation);
    composeFrame();
    present::bootFrame(true);
  } else {
    composeFrame();
    present::show(present::Mode::Full, now);
  }
  g_app.takeFull();
}

static void openSettings(bool ring, uint32_t now) {
  snapshotInfo();
  settings_flow::open(g_settings, g_app.orientation(), ring, now);
  drawSettingsFrame(present::Mode::Full, now);
  Serial.println("APP: settings opened");
}

static void handleSettingsResult(const settings_flow::Result &r, uint32_t now) {
  if (r.refresh) {
    if (g_real && g_drv && !g_sessionOver) {
      g_refreshSeq = g_drv->fetchSeq();
      if (g_drv->refresh(now)) {
        g_settingsRefreshing = true;
      } else {  // the machine ignored it: report failure, never the previous result
        handleSettingsResult(settings_flow::refreshDone(false), now);
        return;
      }
    } else {
      handleSettingsResult(settings_flow::refreshDone(true), now);
      return;
    }
  }
  if (r.left) {
    closeSettings(r.leave, now);
    return;
  }
  if (r.redraw) drawSettingsFrame(present::Mode::Partial, now);
}

void setup() {
  boot::begin();

  settings::Settings &s = g_settings;
  {
    NvsSettingsStore store;
    if (store.begin()) {
      s = settings::load(store);
      store.end();
    }
  }
#ifdef DEMO_LANDSCAPE
  s.orientation = settings::Orientation::Landscape;
#endif
  boot::mark("settings");

  const int rotation = (s.orientation == settings::Orientation::Landscape) ? 1 : 0;
  auto &d = M5.Display;
  // Panel_IT8951 has auto-display on: every outermost endWrite() would refresh the accumulated
  // range at once. All refreshes here are explicit (present.cpp).
  d.getPanel()->setAutoDisplay(false);
  d.setRotation(rotation);
  input::begin();

  g_cfg = app_config::load();
  const app_config::Loaded &cfg = g_cfg;
  applyTimezone(cfg.cfg.timezone);
  boot::log("config_source", cfg.source);
  if (cfg.error) {
    g_app.initError(cfg.errors, s.orientation);
    const auto &e = cfg.errors[0];
    Serial.printf("BOOT: config_error path=%s line=%d msg=%s\n", e.path.c_str(), e.line, e.message.c_str());
  } else {
    // Demo (fake states, local fake actions) unless the config has a connection section.
    g_demoMode = cfg.demo || !cfg.cfg.hasConnection;
    g_real = !g_demoMode;
    g_app.init(cfg.cfg, s.orientation, g_demoMode);
    g_app.setActionSink(onAppAction);
    g_app.setMarksDriven(g_real);
    boot::log("mode", g_real ? "real" : "demo");
  }
#ifdef TEST_CONFIG_MOCKUP
  g_status.mockLabel = true;
  {
    // The mockup shows the group's "up" button pending: freeze it for the comparison.
    g_app.setPendingMs(1000000000u);
    const auto &pl = g_app.layout().widgets[1];
    auto subs = ui::subRegions(pl.type, config::WidgetSize{}, pl.rect);
    g_app.touch({subs[0].rect.x + 5, subs[0].rect.y + 5}, 0);
    g_app.setPendingMs(uistate::kPendingMs);
  }
#endif
  boot::mark("config");

  if (!present::begin()) {
    boot::log("fatal", "frame_alloc_failed");
    present::showError("Out of memory");
    boot::log("version", "017");
    boot::log("settle_bst_end", boot::endBurstRead() ? 1L : 0L);
    boot::setupDone();  // stay up for serial; no frame paths, no crash loop
    return;
  }
  composeFrame();
  boot::mark("render");

#ifdef BOOT_TEST_HANG
  if (BOOT_TEST_HANG >= 2 || !boot::fallbackActive()) {
    boot::log("test_hang", "blocking");
    while (true) delay(100);
  }
#endif
  present::bootFrame(boot::fallbackActive());
  boot::mark("first_draw");

  boot::log("version", "017");
  boot::log("orientation", (long)rotation);
  boot::logHealth();
  // Leave the controller idle: close any read burst before a restart or sleep.
  boot::log("settle_bst_end", boot::endBurstRead() ? 1L : 0L);
  boot::setupDone();

#ifdef TLS_CHECK
  tls_check::run();
#endif
  if (g_real) {
    session_glue::loadClockFromRtc();
    g_net.begin(cfg.cfg.connection.ha.internalUrl, cfg.cfg.connection.ha.externalUrl, cfg.cfg.connection.ha.token);
    startSession(millis());
  }
#ifdef TEST_SETTINGS
  openSettings(true, millis());
#endif
}

#ifdef BOOT_TEST_RESTART
static void testRestart() {
  if (esp_reset_reason() == ESP_RST_POWERON) {
    delay(300);
    ESP.restart();
  }
}
#endif

void loop() {
  if (!present::framesOk()) {
    shot::poll(shot::Hooks{});  // no frames: the dump and mem commands do nothing
    delay(50);
    return;
  }
#ifdef BOOT_TEST_RESTART
  static bool done = false;
  if (!done) { done = true; testRestart(); }
#endif
  const uint32_t now = millis();
  bool changed = false;
  const input::Event e = input::poll();
  if (e.kind != input::Event::None && g_real && g_drv) {
    if (g_sessionOver) startSession(now);  // a new interactive session (real sleep comes later)
    else g_drv->input(now);
  }
  if (settings_flow::active()) {
    if (e.kind == input::Event::Touch) handleSettingsResult(settings_flow::onTouch(e.point, now), now);
    else if (e.kind == input::Event::Key)
      handleSettingsResult(settings_flow::onKey(ui::mapRocker(e.key, g_app.orientation(), false), now), now);
    handleSettingsResult(settings_flow::tick(now), now);
  } else if (e.kind == input::Event::Touch) {
    const ui::Hit hit = g_app.screen() == uistate::Screen::Tabs ? ui::hitTest(g_app.layout(), e.point) : ui::Hit();
    if (hit.kind == ui::HitKind::Settings) {
      openSettings(false, now);
    } else if (hit.kind == ui::HitKind::Refresh) {
      if (g_real && g_drv && !g_drv->fetching() && !g_sessionOver) {
        g_drv->refresh(now);
        changed = true;  // the icon switches to the dot
      }
    } else {
      g_tapNow = now;
      changed |= g_app.touch(e.point, now);
    }
  } else if (e.kind == input::Event::Key) {
    if (e.key == ui::PhysicalKey::LongPush && g_app.screen() == uistate::Screen::Tabs &&
        g_app.focus().mode == ui::FocusMode::Tab) {
      openSettings(true, now);
    } else {
      g_tapNow = now;
      changed |= g_app.rocker(e.key, now);
    }
  }

  if (g_drv) {
    g_drv->tick(now);
    session_glue::ntpPoll(g_drv->wifiUp());
    if (g_drv->takeRender()) changed = true;
    changed |= syncMarks();
    if (!g_stackLogged && g_drv->sessionDone()) {
      g_stackLogged = true;
      Serial.printf("APP: net_stack_free=%u\n", (unsigned)g_net.stackFreeBytes());
    }
    if (g_settingsRefreshing && g_drv->fetchSeq() != g_refreshSeq) {
      g_settingsRefreshing = false;
      const bool ok = !g_drv->lastFetchFailed();
      if (ok) snapshotInfo();
      handleSettingsResult(settings_flow::refreshDone(ok), now);
    }
    app_logic::SleepRequest sl;
    if (g_drv->takeSleep(sl)) {
      // Real sleep comes in a later task: prepare as if, then stay awake until the next input.
      Serial.printf("APP: sleep_planned mode=%d alarm=%d touch_wake=%d\n", int(sl.plan.mode),
                    sl.plan.rtc_alarm_at ? 1 : 0, sl.plan.touch_wake ? 1 : 0);
      if (!settings_flow::active()) present::prepareSleep();
      g_sessionOver = true;
    }
  }

  shot::Hooks hooks;
  hooks.frame = &present::canvas();
  hooks.previous = &present::previous();
  hooks.setPendingMs = [](uint32_t ms) { g_app.setPendingMs(ms); };
  shot::poll(hooks);

  changed |= g_app.tick(now);
  if (changed) showFrame(now);
  present::idle(millis());
  delay(10);
}
