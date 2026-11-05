// Settings screen glue: snapshots, orientation / config reload, firmware row. Split from main.cpp (task 124e).
#include "app_internal.h"

static settings_ui::WallTime wallTime(int64_t epoch) {
  settings_ui::WallTime w;
  if (!app_logic::plausibleEpoch(epoch)) return w;
  const app_logic::Civil c = app_logic::epochToCivil(epoch, utcOffsetAt(epoch));
  w.month = c.month; w.day = c.day; w.hour = c.hour; w.minute = c.minute;
  return w;
}

void snapshotInfo() {
  render::SettingsInfo &i = g_info;
  i.firmware = "v" FW_VERSION;
  i.build = buildinfo::info();
  i.battery.percent = batteryPercent();
  i.battery.millivolts = batteryMv();
  i.battery.learnedLimitMv = batterylimit::learned(g_settings.learn_battery) ? batteryLimitMv() : 0;
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

// ---- settings screen ----

static void applyOrientation(settings::Orientation o) {
  std::vector<EntityState> keep;
  for (const std::string &id : g_cfg.cfg.entityIds)
    if (const EntityState *s = g_app.store().get(id)) keep.push_back(*s);
  M5.Display.setRotation(o == settings::Orientation::Landscape ? 1 : 0);
  present::begin();
  g_app.init(g_app.config(), o, g_demoMode);  // the tabs live in the app only (task 117: see dropCfgTabs)
  g_app.setActionSink(onAppAction);
  g_app.setBrightnessSink(onBrightnessSend);
  g_app.setMediaSink(onMediaSend);
  g_app.setAlarmSink(onAlarmSend);  // task 108
  g_app.setMarksDriven(g_real);
  if (!g_demoMode) g_app.store().update(keep);
  graphsInit();
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
  dropCfgTabs();
  g_app.setActionSink(onAppAction);
  g_app.setBrightnessSink(onBrightnessSend);
  g_app.setMediaSink(onMediaSend);
  g_app.setAlarmSink(onAlarmSend);  // task 108
  g_app.setMarksDriven(g_real);
  graphsInit();
  boot::log("mode", g_real ? "real" : "demo");
  if (g_real) {
    session_glue::loadClockFromRtc();
    if (batteryTooLowForWifi()) lowBatterySleep();  // the brownouts hit when Wi-Fi starts (task 079)
    g_net.begin(g_cfg.cfg.connection.ha.internalUrl, g_cfg.cfg.connection.ha.externalUrl,
                g_cfg.cfg.connection.ha.token);
    startSession(millis());
  }
}

// ---- firmware update row (task 087, docs/ui/settings.md 8.6) ----

static bool usbNow() { return power::usbPresent(); }

static settings_ui::FwCheckErr g_fwCheckErr = settings_ui::FwCheckErr::None;
static bool g_fwJustChecked = false;
static settings_ui::FwDlErr g_fwDlErr = settings_ui::FwDlErr::None;

// The OTA code closes the HA keep-alive socket before a check / download (frees the TLS block).
void otaQuiesce() {
  if (!g_real || !g_net.running()) return;
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Close;
  g_net.post(c);
  for (int i = 0; i < 30; ++i) {  // ~300 ms: the net task closes the socket in order
    boot::feed();
    delay(10);
  }
}

static settings_ui::FirmwareInfo fwSnapshot() {
  settings_ui::FirmwareInfo f;
  f.current = FW_VERSION;
  f.available = ota::availableVersion();
  f.sizeBytes = ota::availableSize();
  f.updatedFrom = ota::updatedFrom();
  f.rolledBack = ota::rolledBack();
  f.lastCheck = wallTime(ota::lastCheckEpoch());
  f.now = wallTime(session_glue::wallClock());
  f.demo = g_settings.demo_mode || !g_real;
  f.wifi = g_drv && g_drv->wifiUp();
  f.blockedBattery = ota::installBlock(usbNow(), batteryPercent()) != ota_logic::InstallBlock::None;
  f.checkErr = g_fwCheckErr;
  f.justChecked = g_fwJustChecked;
  f.dlErr = g_fwDlErr;
  return f;
}

// After a blocking step: the Settings idle timer and the session's idle timer restart.
static void fwResumed() {
  const uint32_t now = millis();
  settings_flow::keepAlive(now);
  if (g_real && g_drv && !g_sessionOver) g_drv->input(now);
  else g_localIdle.on_input(now / 1000);
}

static void fwProgress(ota::Phase ph, int pct) {
  static uint32_t lastDrawMs = 0;
  static int lastPct = -100;
  static ota::Phase lastPh = ota::Phase::Restarting;
  const uint32_t now = millis();
  const bool phaseChanged = ph != lastPh;
  if (!phaseChanged && pct < lastPct + 5 && now - lastDrawMs < 2000) return;  // one partial per 5 % or 2 s
  lastPh = ph;
  lastPct = pct;
  lastDrawMs = now;
  settings_flow::setFirmwareProgress(ph == ota::Phase::Downloading ? settings_ui::FwPhase::Downloading
                                     : ph == ota::Phase::Verifying ? settings_ui::FwPhase::Verifying
                                                                   : settings_ui::FwPhase::Restarting, pct);
  drawSettingsFrame(present::Mode::Partial, now);
  if (ph == ota::Phase::Restarting) delay(300);  // let the partial finish before the restart
}

static void fwCheck(uint32_t now) {
  drawSettingsFrame(present::Mode::Partial, now);  // Checking... look
  const ota::CheckResult res = ota::checkNow(session_glue::wallClock(), g_drv && g_drv->wifiUp());
  g_fwJustChecked = res == ota::CheckResult::UpToDate;
  using E = settings_ui::FwCheckErr;
  g_fwCheckErr = res == ota::CheckResult::NoWifi    ? E::NoWifi
                 : res == ota::CheckResult::Timeout ? E::Timeout
                 : (res == ota::CheckResult::NoServer || res == ota::CheckResult::Skipped) ? E::NoServer
                                                                                            : E::None;
  settings_flow::clearFirmwareBusy();
  settings_flow::setFirmware(fwSnapshot());
  fwResumed();
  drawSettingsFrame(present::Mode::Partial, millis());
}

static void fwInstall(uint32_t now) {
  if (settings_flow::screen().dirty()) {  // the restart must not lose the edit buffer
    NvsSettingsStore store;
    if (store.begin()) {
      settings::save(store, settings_flow::screen().edit());
      store.end();
    }
  }
  g_fwDlErr = settings_ui::FwDlErr::None;
  settings_flow::setFirmware(fwSnapshot());
  drawSettingsFrame(present::Mode::Full, now);  // Tools page, the row in its downloading look
  wakelog::logf("OTA: install start version=%s\n", ota::availableVersion().c_str());
  ota::install(fwProgress);  // returns only on failure; the running firmware is untouched
  g_fwDlErr = ota::failure() == ota::Failure::Damaged   ? settings_ui::FwDlErr::Damaged
              : ota::failure() == ota::Failure::TooLong ? settings_ui::FwDlErr::TooLong
                                                        : settings_ui::FwDlErr::Download;
  settings_flow::clearFirmwareBusy();
  settings_flow::setFirmware(fwSnapshot());
  fwResumed();
  drawSettingsFrame(present::Mode::Full, millis());  // failed look + one clean-up of the page
}


bool actionPending() {
  if (!g_drv) return false;
  for (const auto &kv : g_drv->marks())
    if (kv.second == session::Mark::Pending) return true;
  return false;
}

static void closeSettings(const settings_ui::LeaveResult &r, uint32_t now) {
  ota::clearNotices();  // "Updated from" / "rolled back" show on the first Settings visit only
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
    present::setLogTab(g_app.shownTab());
    present::bootFrame(true);  // everything changed: full redraw
  } else if (r.orientationChanged) {
    applyOrientation(r.settings.orientation);
    composeFrame();
    present::setLogTab(g_app.shownTab());
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

void openSettings(bool ring, uint32_t now) {
  g_defer.cancel();
  snapshotInfo();
  settings_flow::open(g_settings, g_app.orientation(), ring, now);
  settings_flow::setNetwork(networkInfo());
  g_fwCheckErr = settings_ui::FwCheckErr::None;
  g_fwJustChecked = false;
  g_fwDlErr = settings_ui::FwDlErr::None;
  settings_flow::setFirmware(fwSnapshot());
  drawSettingsFrame(present::Mode::Full, now);
  wakelog::logf("APP: settings opened\n");
}


void handleSettingsResult(const settings_flow::Result &r, uint32_t now) {
  if (r.fwCheck) {
    fwCheck(now);
    return;
  }
  if (r.fwInstall) {
    fwInstall(now);
    return;
  }
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
  if (r.sleepNow) {
    manualSleep();  // leaves Settings through goToSleep's auto-leave, then the normal sleep path
    return;
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
