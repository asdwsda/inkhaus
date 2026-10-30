#include <M5Unified.h>

#include <time.h>

#include <map>
#include <memory>
#include <vector>

#include "app_config.h"
#include "demo_mode.h"
#include "app_state.h"
#include "config_sd.h"
#include "boot.h"
#include "brightness_intent.h"
#include "history_cache_sd.h"
#include "deferred_tap.h"
#include "input.h"
#include "net.h"
#include "net_task.h"
#include "present.h"
#include "render/screen.h"
#include "render/settings_screen.h"
#include "render/splash_screen.h"
#include "session_driver.h"
#include "session_glue.h"
#include "settings.h"
#include "settings_flow.h"
#include "settings_nvs.h"
#include "sleep.h"
#include "state_cache.h"
#include "sleep_logic.h"
#include "shot.h"
#include "tap_intent.h"
#include "version.h"
#include "wakelog.h"
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

// present::show with the wake log's refresh count and time (task 029; no-op unless the debug log is on).
static bool timedShow(present::Mode mode, uint32_t now) {
  const uint32_t t0 = millis();
  const bool did = present::show(mode, now);
  if (did) wakelog::refreshed(millis() - t0);
  return did;
}

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
static power::IdleTimer g_localIdle;  // sleep timer when no session runs (demo, config error)
static bool g_localIdleOn = false;
static bool g_sessionOver = false;  // the machine asked to sleep; any input starts a new session
static bool g_settingsRefreshing = false;
static uint32_t g_refreshSeq = 0;  // fetchSeq() when the Settings refresh was issued
static bool g_stackLogged = false;
static bool g_firstSession = true;
static int64_t g_restoredEpoch = 0;  // task 037: when the restored values were last confirmed (0 = none)
static power::WakeReason g_wake = power::WakeReason::ColdBoot;  // why this run started (sleepwake::detect)
static bool g_scheduled = false;                                // scheduled refresh: no touch/rocker input
static app_logic::WakeInputGuard g_wakeGuard;                   // swallows the press that woke the device

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

// Status bar phase (STYLE.md 5.3): never blank. Values restored from the cache (g_restoredEpoch, task 037)
// count as the last good update until a fetch result replaces them.
static void updateStatus() {
  g_status.batteryPct = batteryPercent();
  using P = app_logic::Phase;
  P phase = P::Demo;
  int64_t lastOk = 0;
  bool failed = false;
  if (g_real) {
    phase = P::ConnectingWifi;  // before the session driver exists (first frame)
    if (g_drv) {
      lastOk = g_drv->lastOkEpoch() ? g_drv->lastOkEpoch() : g_restoredEpoch;
      failed = g_drv->lastFetchFailed();
      switch (g_drv->status()) {
        case session::Status::Connecting: phase = g_drv->wifiUp() ? P::Fetching : P::ConnectingWifi; break;
        case session::Status::Online: phase = g_drv->fetching() ? P::Fetching : P::Online; break;
        case session::Status::Offline: phase = P::Offline; break;
        case session::Status::AuthError: phase = P::AuthError; break;
      }
    } else {
      lastOk = g_restoredEpoch;
    }
  } else if (!g_demoMode) {
    return;
  }
  using L = render::Link;
  switch (phase) {
    case P::ConnectingWifi: g_status.link = L::ConnectingWifi; break;
    case P::Fetching: g_status.link = L::Fetching; break;
    case P::Online: g_status.link = L::Online; break;
    case P::Offline: g_status.link = L::Offline; break;
    case P::AuthError: g_status.link = L::AuthError; break;
    case P::Demo: g_status.link = L::Demo; break;
  }
  const int64_t nowEpoch = session_glue::wallClock();
  const app_logic::Label l = app_logic::statusLabel(phase, lastOk, nowEpoch, utcOffsetAt(nowEpoch),
                                                    g_settings.refresh_interval_min, failed, landscape());
  g_status.label1 = l.line1;
  g_status.label2 = l.line2;
  g_status.outdated = l.stale;
}

static render::SleepIndicator toRender(app_logic::Indicator i) {
  return i == app_logic::Indicator::Button  ? render::SleepIndicator::Button
         : i == app_logic::Indicator::Touch ? render::SleepIndicator::Touch
                                            : render::SleepIndicator::None;
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
  i.firmware = "v" FW_VERSION;
  i.battery.percent = batteryPercent();
  i.battery.millivolts = batteryMv();
  i.battery.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  i.wifi.configured = g_real;
  i.wifi.connected = g_drv && g_drv->wifiUp();
  i.wifi.ssid = g_cfg.cfg.connection.wifi.ssid;  // task 047: the connected profile's SSID when there is one
  if (i.wifi.connected) {
    const std::string active = net::activeProfileName();
    for (const auto &p : g_cfg.cfg.connection.wifiProfiles)
      if (p.name == active) i.wifi.ssid = p.ssid;
  }
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

// Snapshot for the Settings > Network page (profile names for cycling, live hints).
static settings_ui::NetworkInfo networkInfo() {
  settings_ui::NetworkInfo n;
  for (const auto &p : g_cfg.cfg.connection.wifiProfiles) n.profiles.push_back(p.name);
  n.demo = g_settings.demo_mode || !g_real;
  if (n.demo) return n;
  if (g_drv && g_drv->wifiUp()) n.connected = net::activeProfileName();
  n.profileDecides = net::linkInfo().haUrl != 0;  // config::HaUrlMode::Auto == 0
  if (g_drv && g_drv->sessionDone()) {
    n.link = g_drv->lastFetchFailed() ? settings_ui::LinkState::None
             : g_drv->lastLinkExternal() ? settings_ui::LinkState::External
                                         : settings_ui::LinkState::Internal;
  }
  return n;
}

static void drawSettingsFrame(present::Mode mode, uint32_t now) {
  render::drawSettings(present::canvas(), settings_flow::screen(), g_info);
  timedShow(mode, now);
}

// Diagnostics: after each fetch result, how many shown-tab widget members have no state.
static void logFetchApplied() {
  static uint32_t seen = 0;
  if (!g_drv || g_drv->fetchSeq() == seen) return;
  seen = g_drv->fetchSeq();
  int missing = 0;
  const auto &tabs = g_app.config().tabs;
  const int tab = g_app.shownTab();
  if (tab >= 0 && size_t(tab) < tabs.size()) {
    for (size_t i = 0; i < tabs[size_t(tab)].widgets.size(); ++i)
      for (const auto *m : g_app.widgetData(int(i)).members)
      {
        if (!m) { ++missing; continue; }
        if (m->available) continue;
        char st[33];
        snprintf(st, sizeof st, "%s", m->state.c_str());
        if (m->current_position)
          Serial.printf("APP: unavailable id=%s state=%s pos=%d\n", m->id.c_str(), st, *m->current_position);
        else
          Serial.printf("APP: unavailable id=%s state=%s pos=-\n", m->id.c_str(), st);
      }
  }
  wakelog::logf("APP: fetch_applied store=%u changed=%u missing_members=%d\n",
                (unsigned)g_app.store().size(), (unsigned)g_drv->lastChanged(), missing);
}

static void showFrame(uint32_t now) {
  if (settings_flow::active()) {
    drawSettingsFrame(present::Mode::Partial, now);
    return;
  }
  composeFrame();
  const bool full = g_app.takeFull();
  present::setLogTab(g_app.shownTab());
  // First frame of the history popup's Loading state: hint the fastest mode over the greys, once.
  static bool s_loadingHinted = false;
  const uistate::HistoryPopup &hp = g_app.historyPopup();
  const bool loadingVisible = hp.state() == uistate::HistState::Loading && hp.visible();
  if (loadingVisible && !s_loadingHinted) present::fastestOverGreysOnce();
  s_loadingHinted = loadingVisible;
  timedShow(full ? present::Mode::Full : present::Mode::Partial, now);
}

// ---- history popup (task 053, docs/ui/history.md) ----
// The popup model lives in AppState; this glue does the cache lookup, hands the fetch to the net task and
// feeds the result back. PSRAM map + SD files per entity (lib/history cache_store).
static history_cache_sd::Backend g_histSd;
static history::CacheStore g_histCache(&g_histSd);
static std::shared_ptr<volatile bool> g_histCancel;  // set when the popup closes during a fetch
static uistate::HistoryJob g_histJob;
static bool g_histWaitNet = false;  // a job is planned but Wi-Fi is not up yet
static bool g_histChanged = false;  // a result arrived: redraw

static uint32_t histHash() { return history::paramsHash(g_app.history().spec().params); }

static uistate::HistOutcome histOutcome(HaError e, size_t bytes, bool noStatistics) {
  if (noStatistics) return uistate::HistOutcome::NoStatistics;
  if (app_logic::histTooMany(e, bytes)) return uistate::HistOutcome::TooMany;  // the 512 KB / 15 s limits
  switch (e) {
    case HaError::None: return uistate::HistOutcome::Ok;
    case HaError::Cancelled: return uistate::HistOutcome::Cancelled;
    case HaError::Parse: return uistate::HistOutcome::Unavailable;
    default: return uistate::HistOutcome::Unreachable;
  }
}

static void onHistoryDone(app_logic::NetEvent &&ev) {
  uistate::HistoryPopup &h = g_app.historyPopup();
  const int64_t epoch = session_glue::wallClock();
  const uistate::HistOutcome o = histOutcome(ev.error, ev.bytes, ev.noStatistics);
  const std::string id = h.entity();
  if (h.onFetchDone(ev.ticket, o, std::move(ev.series), epoch)) {
    g_histChanged = true;
    if (o == uistate::HistOutcome::Ok && h.state() == uistate::HistState::Ready)
      g_histCache.put(id, histHash(), h.series(), epoch);  // UNVERIFIED: the SD write (~tens of ms) is fine here
  }
}

// Just after the popup opened: cache lookup (shown at once when found), then plan the fetch.
static void historyOpened() {
  uistate::HistoryPopup &h = g_app.historyPopup();
  history::Series s;
  int64_t at = 0;
  const history::Freshness f =
      g_histCache.lookup(h.entity(), histHash(), session_glue::wallClock(), h.spec().hours, s, at);
  h.applyCache(f, s, at);
  g_histWaitNet = h.takeFetch(session_glue::wallClock(), g_histJob);
}

// Every loop: starts a planned fetch once Wi-Fi is up, fails it when there is no way to get it, and aborts a
// running one when the popup was closed. The popup being open keeps the session awake (see loop()).
static void historyPump() {
  if (g_app.historyPopup().takeCancel()) {
    if (g_histCancel) *g_histCancel = true;
    g_histWaitNet = false;
  }
  if (!g_histWaitNet) return;
  if (!g_real || !g_drv || g_drv->status() == session::Status::Offline ||
      g_drv->status() == session::Status::AuthError) {
    g_histWaitNet = false;
    if (g_app.historyPopup().fetchUnavailable()) g_histChanged = true;
    return;
  }
  if (!g_drv->wifiUp()) return;
  g_histWaitNet = false;
  g_histCancel = std::make_shared<volatile bool>(false);
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::History;
  c.ticket = g_histJob.ticket;
  c.histEntity = g_histJob.entity;
  c.histStart = g_histJob.start;
  c.histEnd = g_histJob.end;
  c.histColumns = g_histJob.columns;
  c.histStats = g_histJob.statistics;
  c.histBar = g_histJob.bar;
  c.histPeriod = g_histJob.period;
  c.histStatistic = g_histJob.statistic;
  c.histBucketSeconds = g_histJob.bucketSeconds;
  c.histBucketCount = g_histJob.bucketCount;
  c.histCancel = g_histCancel;
  g_drv->postHistory(c);
}

// Closes the popup and aborts its fetch (before sleep, session restart).
static void historyShutdown() {
  g_app.closeHistory();
  historyPump();
}

// ---- session ----

static power::WakeReason g_sessionWake = power::WakeReason::Button;  // the wake reason the session was planned with
static bool g_netRestartDue = false;  // task 047: a network change waits for a pending user action
static uint32_t g_netRestartSince = 0;

// Names of the secrets.yaml Wi-Fi profiles, list order (the forced profile is stored by name).
static std::vector<std::string> profileNames() {
  std::vector<std::string> v;
  for (const auto &p : g_cfg.cfg.connection.wifiProfiles) v.push_back(p.name);
  return v;
}

// restart: the same session again (network change): keep its wake reason, so the sleep plan is unchanged.
static void startSession(uint32_t now, bool restart = false) {
  g_net.reset();
  session_glue::ntpSessionStart();
  g_drv.reset();  // before the Wi-Fi adapter it points to
  net::configure(g_cfg.cfg.connection.wifiProfiles,
                 settings::wifiProfileIndex(g_settings.wifi_profile, profileNames()), (int)g_settings.ha_link);  // task 047
  g_wifi.reset(new session_glue::ArduinoWifi(g_cfg.cfg.connection.wifi));
  g_drv.reset(new app_logic::SessionDriver(g_cfg.cfg.entityIds, g_app.store(), *g_wifi, g_net,
                                           session_glue::wallClock, appLog));
  g_sessionOver = false;
  g_drv->setHistoryHandler(onHistoryDone);
  if (g_app.history().fetching() || g_histWaitNet) {  // the old net session is gone: its result will not arrive
    g_histWaitNet = false;
    if (g_histCancel) *g_histCancel = true;
    if (g_app.historyPopup().fetchUnavailable()) g_histChanged = true;
  }
  wakelog::sessionStarted(now);
  // The offset is taken now (local time at planning time); the machine plans the wake-up from it.
  if (!restart) g_sessionWake = g_firstSession ? g_wake : power::WakeReason::Button;
  g_drv->start(g_sessionWake, batteryMv(), g_settings, utcOffsetAt(session_glue::wallClock()), now);
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

// Brightness popup send (task 045): light.turn_on with brightness_pct, or light.turn_off for 0.
static void onBrightnessSend(const std::string &id, int pct) {
  if (!g_real || !g_drv) return;
  auto ev = app_logic::brightnessIntent(id, pct);
  if (ev) g_drv->userAction(*ev, millis());
}

// A tap on a dimmable light waits for the release or the long press (task 045).
static uistate::DeferredTap g_defer;

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
  g_app.setBrightnessSink(onBrightnessSend);
  g_app.setMarksDriven(g_real);
  if (!g_demoMode) g_app.store().update(keep);
  boot::log("orientation", (long)(o == settings::Orientation::Landscape ? 1 : 0));
}

// Demo mode switched in Settings: drop the sessions, load the other config source, rebuild the app
// state for orientation `o` and, when the dashboard is real, start a session.
static void reloadConfigSource(settings::Orientation o) {
  session_glue::ntpAbort();
  g_drv.reset();  // before the Wi-Fi adapter it points to
  g_wifi.reset();
  g_net.reset();
  net::powerDown();
  g_sessionOver = false;
  g_netRestartDue = false;
  g_real = false;
  g_demoMode = true;
  g_cfg = app_config::load(app_logic::chooseConfigSource(g_settings));
  boot::log("config_source", g_cfg.source);
  applyTimezone(g_cfg.cfg.timezone);
  M5.Display.setRotation(o == settings::Orientation::Landscape ? 1 : 0);
  present::begin();
  if (g_cfg.error) {
    g_app.initError(g_cfg.errors, o);
    return;
  }
  g_demoMode = app_logic::isDemoRun(g_settings, g_cfg.demo, g_cfg.cfg.hasConnection);
  g_real = !g_demoMode;
  g_app.init(g_cfg.cfg, o, g_demoMode);
  g_app.setActionSink(onAppAction);
  g_app.setBrightnessSink(onBrightnessSend);
  g_app.setMarksDriven(g_real);
  boot::log("mode", g_real ? "real" : "demo");
  if (g_real) {
    session_glue::loadClockFromRtc();
    g_net.begin(g_cfg.cfg.connection.ha.internalUrl, g_cfg.cfg.connection.ha.externalUrl,
                g_cfg.cfg.connection.ha.token);
    startSession(millis());
  }
}

static constexpr uint32_t kNetRestartWaitMs = 10000;  // above the HTTP timeouts (4 s connect, 8 s total)

static bool actionPending() {
  if (!g_drv) return false;
  for (const auto &kv : g_drv->marks())
    if (kv.second == session::Mark::Pending) return true;
  return false;
}

static void closeSettings(const settings_ui::LeaveResult &r, uint32_t now) {
  g_settingsRefreshing = false;
  if (r.save) {
    NvsSettingsStore store;
    if (store.begin()) {
      settings::save(store, r.settings);
      store.end();
    }
    wakelog::logf("APP: settings saved\n");
  }
  g_settings = r.settings;
  wakelog::setEnabled(r.settings.debug_log);
  if (r.demoChanged) {
    reloadConfigSource(r.settings.orientation);
    composeFrame();
    present::bootFrame(true);  // everything changed: full redraw
  } else if (r.orientationChanged) {
    applyOrientation(r.settings.orientation);
    composeFrame();
    present::bootFrame(true);
  } else {
    composeFrame();
    timedShow(present::Mode::Full, now);
  }
  g_app.takeFull();
  // Task 047: a changed Wi-Fi profile or HA connection applies at once: a fresh session, after the tab
  // screen is drawn. (A demo switch already started its own session in reloadConfigSource.) A user action
  // still in flight finishes first (loop(), up to kNetRestartWaitMs).
  if (r.networkChanged && !r.demoChanged && g_real) {
    g_netRestartDue = true;
    g_netRestartSince = now;
  }
}

static void openSettings(bool ring, uint32_t now) {
  g_defer.cancel();
  snapshotInfo();
  settings_flow::open(g_settings, g_app.orientation(), ring, now);
  settings_flow::setNetwork(networkInfo());
  drawSettingsFrame(present::Mode::Full, now);
  wakelog::logf("APP: settings opened\n");
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
    } else {  // demo (also saved demo mode): no fetch; Refresh now takes its failed look
      handleSettingsResult(settings_flow::refreshDone(false), now);
      return;
    }
  }
  if (r.left) {
    closeSettings(r.leave, now);
    return;
  }
  if (r.pageChanged) {  // main <-> Tools: GC16 full refresh (spec section 5, "Refreshes")
    if (r.toolsOpened) {  // is a card present? (the Debug log note says so); mount is paired with unmount
      const bool sd = config_sd::mount();
      if (sd) config_sd::unmount();
      settings_flow::setSdPresent(sd);
    }
    drawSettingsFrame(present::Mode::Full, now);
    return;
  }
  if (r.redraw) drawSettingsFrame(present::Mode::Partial, now);
}

// ---- sleep (task 024) ----

// Shared tail of every sleep: indicator frame (not for the low-battery notice), Wi-Fi off, controller
// idle, then the real sleep. Returns only if sleeping failed.
static void goToSleep(power::SleepPlan plan, power::Session session, bool localIdle = false) {
  uint32_t now = millis();
  const bool low = session == power::Session::LowBattery;
  session_glue::ntpAbort();
  if (!low) {
    historyShutdown();
    g_app.closeBrightness(now);  // flush the unsent value; the frozen screen never shows the popup (brightness.md rule 5)
    // Settings auto-leave before sleeping (settings.md rule 5). Whether or not Settings was open now,
    // the plan is made again from the settings in effect now: the session machine's plan was made
    // from the copy taken when the session started (task 039 bug).
    if (settings_flow::active()) handleSettingsResult(settings_flow::tick(now + 3600u * 1000u), now);
    const int64_t epoch = session_glue::wallClock();
    plan = app_logic::planForSleep(g_settings, session, epoch, utcOffsetAt(epoch), localIdle);
  }
  if (!low && present::framesOk()) {
    present::prepareSleep();  // ghost debt first, the indicator is drawn last (STYLE.md 5.5)
    g_status.sleepIndicator = toRender(app_logic::indicatorFor(plan, session));
    composeFrame();
    now = millis();
    timedShow(present::Mode::Partial, now);
    present::prepareSleep();  // saves the record of the frame with the indicator, closes the read state
  }
  if (g_real) state_cache::save(g_app.store(), g_drv ? g_drv->lastOkEpoch() : 0);
  net::powerDown();
  boot::endBurstRead();
  boot::setupDone();
  sleepwake::enter(plan, session, session_glue::wallClock(), batteryMv());
  // Not asleep (should not happen): carry on as an awake device without the indicator.
  boot::log("sleep_failed", 1);
  g_status.sleepIndicator = render::SleepIndicator::None;
}

// Battery below the limit at wake: no session, no network. A plain notice, no alarm, power off.
static void lowBatterySleep() {
  boot::log("low_battery_mv", (long)batteryMv());
  session_glue::loadClockFromRtc();
  const int64_t epoch = session_glue::wallClock();
  present::showNotice("Battery low", "Please charge the device");
  goToSleep(power::plan_sleep(g_settings, power::Session::LowBattery, epoch, 0), power::Session::LowBattery);
}

// Identifies this firmware build for the screen record (a record of another build is not trusted).
static uint32_t firmwareId() {
  uint32_t h = 2166136261u;
  for (const char *p = FW_VERSION " " __DATE__ " " __TIME__; *p; ++p) h = (h ^ uint8_t(*p)) * 16777619u;
  return h ? h : 1u;
}

void setup() {
  boot::begin();
  present::setFirmwareId(firmwareId());
  g_wake = sleepwake::detect();
  const power::Session kind = power::decide_session(g_wake, batteryMv());
  g_scheduled = kind == power::Session::ScheduledRefresh;
  settings::Settings &s = g_settings;
  {
    NvsSettingsStore store;
    if (store.begin()) {
      s = settings::load(store);
      store.end();
    }
  }
  wakelog::setEnabled(s.debug_log);
  wakelog::begin(app_logic::wakeName(g_wake), kind);
  if (g_scheduled) g_status.sleepIndicator = toRender(sleepwake::storedIndicator());

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
  if (kind == power::Session::LowBattery) lowBatterySleep();  // returns only if sleeping failed
  if (!g_scheduled) input::begin();  // a scheduled refresh never initialises touch input

  // Cold boot splash (docs/ui/splash.md): power-on, reset button or flashing. One GC16 refresh, then the rest of setup runs.
  bool framesReady = false, splashed = false;
  uint32_t splashMs = 0;
  {
    const esp_reset_reason_t rr = esp_reset_reason();
    if (!boot::fallbackActive() && (rr == ESP_RST_POWERON || rr == ESP_RST_EXT) && present::begin()) {
      framesReady = true;
      // Every POWERON/EXT reset shows the splash (deep-sleep wakes never get here); the app then follows
      // with a full redraw (bootFrame(... || splashed) below).
      render::drawSplash(present::canvas(), "v" FW_VERSION);
      present::bootFrame(true);
      splashMs = millis();
      splashed = true;
      wakelog::logf("BOOT: splash=1\n");
    }
  }

  g_cfg = app_config::load(app_logic::chooseConfigSource(s));
  {  // one-time move of the old forced-profile index to the profile name (needs the profile list)
    if (settings::migrateWifiProfile(s, profileNames())) {
      NvsSettingsStore store;
      if (store.begin()) {
        store.setStr(settings::kKeyWifiProfile, s.wifi_profile);
        store.end();
      }
    }
  }
  const app_config::Loaded &cfg = g_cfg;
  applyTimezone(cfg.cfg.timezone);
  boot::log("config_source", cfg.source);
  if (cfg.error) {
    g_app.initError(cfg.errors, s.orientation);
    const auto &e = cfg.errors[0];
    wakelog::logf("BOOT: config_error path=%s line=%d msg=%s\n", e.path.c_str(), e.line, e.message.c_str());
  } else {
    // Demo (fake states, local fake actions) unless the config has a connection section.
    g_demoMode = app_logic::isDemoRun(s, cfg.demo, cfg.cfg.hasConnection);
    g_real = !g_demoMode;
    g_app.init(cfg.cfg, s.orientation, g_demoMode);
    g_app.setActionSink(onAppAction);
  g_app.setBrightnessSink(onBrightnessSend);
    g_app.setMarksDriven(g_real);
    boot::log("mode", g_real ? "real" : "demo");
    if (g_real) g_restoredEpoch = state_cache::restore(cfg.cfg.entityIds, g_app.store());
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

  if (!framesReady && !present::begin()) {
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
  // The splash holds for at least 1.5 s (the config/clock start-up above counts towards it).
  while (splashed && millis() - splashMs < 1500) {
    boot::feed();
    delay(10);
  }
  present::bootFrame(boot::fallbackActive() || splashed);  // after the splash: ghost-free full redraw
  boot::mark("first_draw");

  boot::log("version", "017");
  boot::log("orientation", (long)rotation);
  boot::logHealth();
  // Leave the controller idle: close any read burst before a restart or sleep.
  boot::log("settle_bst_end", boot::endBurstRead() ? 1L : 0L);
  boot::setupDone();
  g_wakeGuard.arm(g_wake, millis());  // last: the 3 s cap counts from when input starts

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
  input::Event e = g_scheduled ? input::Event() : input::poll();
  if (!g_scheduled) {
    g_wakeGuard.observe(now, M5.BtnB.isPressed(), M5.Touch.getCount() > 0);
    if ((e.kind == input::Event::Key && g_wakeGuard.blocksKey()) ||
        (e.kind == input::Event::Touch && g_wakeGuard.blocksTouch()))
      e = input::Event();  // the press that woke the device
  }
  const bool popupOpen = g_app.brightnessOpen() || g_app.historyPopup().fetching();  // activity: brightness closes itself after 10 s; an open history popup only counts while it fetches, then the idle timer closes it (goToSleep)
  if ((e.kind != input::Event::None || popupOpen) && g_real && g_drv) {
    if (g_sessionOver) startSession(now);  // a new interactive session (real sleep comes later)
    else g_drv->input(now);
  }
  if (app_logic::localIdleSleepWanted(g_real && g_drv)) {  // demo / config error: no session machine
    if (!g_localIdleOn) { g_localIdle = power::IdleTimer(g_settings.idle_timeout_s); g_localIdle.start(now / 1000); g_localIdleOn = true; }
    if (e.kind != input::Event::None || settings_flow::active() || popupOpen) g_localIdle.on_input(now / 1000);
    if (g_localIdle.should_sleep(now / 1000)) {
      const int64_t epoch = session_glue::wallClock();
      goToSleep(app_logic::localIdlePlan(g_settings, epoch, utcOffsetAt(epoch)), power::Session::Interactive, true);
      g_localIdle.on_input(millis() / 1000);  // sleeping failed: try again later
    }
  }
  if (settings_flow::active()) {
    if (e.kind == input::Event::Touch) handleSettingsResult(settings_flow::onTouch(e.point, now), now);
    else if (e.kind == input::Event::Key)
      handleSettingsResult(settings_flow::onKey(ui::mapRocker(e.key, g_app.orientation(), false), now), now);
    handleSettingsResult(settings_flow::tick(now), now);
  } else if (e.kind == input::Event::LongTouch) {
    const auto d = g_defer.longPress();
    if (d.kind == uistate::DeferredTap::Result::LongPress) changed |= g_app.longPress(d.point, now);
    else if (g_app.longPress(e.point, now)) {  // a sensor / binary widget: the history popup
      historyOpened();
      changed = true;
    }
  } else if (e.kind == input::Event::TouchRelease) {
    const auto d = g_defer.release();  // a short tap on a dimmable light: on/off as usual
    if (d.kind == uistate::DeferredTap::Result::Tap) {
      g_tapNow = now;
      changed |= g_app.touch(d.point, now);
    }
  } else if (e.kind == input::Event::Touch && g_app.modalOpen()) {
    g_tapNow = now;
    changed |= g_app.touch(e.point, now);  // modal: outside closes, controls act
  } else if (e.kind == input::Event::Touch && g_app.screen() == uistate::Screen::Tabs &&
             g_app.dimmableLightAt(e.point)) {
    g_defer.press(e.point, now);
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
    if (e.key == ui::PhysicalKey::LongPush && !g_app.modalOpen() && g_app.screen() == uistate::Screen::Tabs &&
        g_app.focus().mode == ui::FocusMode::Tab) {
      openSettings(true, now);
    } else if (e.key == ui::PhysicalKey::LongPush && !g_app.modalOpen() && g_app.openBrightnessFocused(now)) {
      g_defer.cancel();
      changed = true;
    } else if ((e.key == ui::PhysicalKey::LongPush || e.key == ui::PhysicalKey::Push) && !g_app.modalOpen() &&
               g_app.screen() == uistate::Screen::Tabs && g_app.openHistoryFocused(now)) {
      historyOpened();
      changed = true;
    } else {
      g_tapNow = now;
      changed |= g_app.rocker(e.key, now);
    }
  }

  if (g_netRestartDue && g_real && !settings_flow::active() &&
      (!actionPending() || now - g_netRestartSince > kNetRestartWaitMs)) {
    g_netRestartDue = false;
    startSession(now, true);  // UNVERIFIED: net::begin restarts a running connect (disconnects first)
  }

  if (g_drv) {
    g_drv->tick(now);
    session_glue::ntpPoll(g_drv->wifiUp());
    if (g_drv->takeRender()) changed = true;
    logFetchApplied();
    if (g_drv->status() == session::Status::Online) wakelog::wifiUp(now);
    {
      static uint32_t seenFetch = 0;
      if (g_drv->fetchSeq() != seenFetch) {
        seenFetch = g_drv->fetchSeq();
        wakelog::fetchDone(now, !g_drv->lastFetchFailed());
      }
    }
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
      session_glue::ntpAbort();
      goToSleep(sl.plan, sl.session);  // does not return when it sleeps
      g_sessionOver = true;
      changed = true;
    }
  }

  historyPump();
  if (g_histChanged) { g_histChanged = false; changed = true; }

  if (!settings_flow::active() && g_app.screen() == uistate::Screen::Tabs) {
    // A phase change repaints only the label region: the frame diff leaves everything else alone.
    const render::StatusInfo before = g_status;
    updateStatus();
    if (before.link != g_status.link || before.label1 != g_status.label1 || before.label2 != g_status.label2 ||
        before.outdated != g_status.outdated)
      changed = true;
  }

  shot::Hooks hooks;
  hooks.frame = &present::canvas();
  hooks.previous = &present::previous();
  hooks.setPendingMs = [](uint32_t ms) { g_app.setPendingMs(ms); };
  shot::poll(hooks);

  const auto late = g_defer.tick(now);  // neither release nor hold seen: treat as a tap
  if (late.kind == uistate::DeferredTap::Result::Tap) {
    g_tapNow = now;
    changed |= g_app.touch(late.point, now);
  }
  changed |= g_app.tick(now);
  if (changed) showFrame(now);
  present::idle(millis());
  delay(10);
}
