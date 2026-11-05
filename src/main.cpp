// Setup and loop orchestration. The app glue lives in app_*.cpp (task 124e); see app_internal.h and app_loop.h.
#include "app_internal.h"
#include "app_loop.h"

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

// Identifies this firmware build for the screen record (a record of another build is not trusted).
static uint32_t firmwareId() {
  uint32_t h = 2166136261u;
  for (const char *p = FW_VERSION " " __DATE__ " " __TIME__; *p; ++p) h = (h ^ uint8_t(*p)) * 16777619u;
  return h ? h : 1u;
}

// Task 117: the config reload (libyaml recursion) overflowed the 8 KB default loop stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
  // Task 117: blocks of 512 bytes and more go to PSRAM (the IDF default is 4096). Internal RAM is the scarce
  // resource (a big dashboard left too little for the Wi-Fi stack); the vectors of widgets, entity states and
  // graph series are such blocks. DMA, Wi-Fi and TLS buffers ask for internal memory explicitly.
  heap_caps_malloc_extmem_enable(512);
  ota::earlyBoot();  // task 087: count a boot of a just-installed firmware first, so a crash in setup() counts
  boot::begin();
  ota::logEarly();
  Serial.printf("BOOT: fw=%s\n", buildinfo::line());  // task 113
  present::setFirmwareId(firmwareId());
  g_wake = sleepwake::detect();
  settings::Settings &s = g_settings;
  {
    NvsSettingsStore store;
    if (store.begin()) {
      s = settings::load(store);
      store.end();
    }
  }
  batterylimit::begin(s.learn_battery, batteryMv());  // task 079: records a brownout event, logs the limit
  const power::Session kind = power::decide_session(g_wake, batteryMv(), batteryLimitMv());
  g_scheduled = kind == power::Session::ScheduledRefresh;
  wakelog::setEnabled(s.debug_log);
  // Early wake feedback (task 058): a touch/rocker wake that finds our sleep badge on the panel clears it
  // at once. The pill is 104x44 inside the badge rect (2 px margin); its x is 4-aligned in both orientations.
  const ui::Rect badgeRect = ui::sleepBadgeRect(s.orientation);
  const ui::Rect pill{badgeRect.x + 2, badgeRect.y + 2, 104, 44};
  const bool earlyClear = !g_scheduled && kind == power::Session::Interactive && sleepwake::badgeOnPanel() &&
                          pill.x % 4 == 0;
  // Task 100: early, before Wi-Fi heats the chip. Reads the SHT30 only when this wake reports.
  report_glue::wakeStart(s.report_ha && !s.demo_mode, g_scheduled, g_wake == power::WakeReason::ColdBoot,
                         g_wake == power::WakeReason::Touch ? devreport::WakeKind::Touch
                         : g_wake == power::WakeReason::Button ? devreport::WakeKind::Button
                                                               : devreport::WakeKind::Boot,
                         s.sleep_screen && !s.demo_mode);  // task 101: the sleep screen shows the reading either way
  wakelog::begin(app_logic::wakeName(g_wake), kind, earlyClear);  // earlyClear: the SD write waits for the first draw
  if (g_scheduled) {  // the pill of our previous sleep stays on the panel (line 2 is replaced when we sleep again)
    g_status.sleepKind = int(sleepwake::storedIndicator());
    const int64_t nx = sleepwake::panelNextAt();
    g_status.sleepNextMin = nx > 0 ? app_logic::localMinuteOfDay(nx, utcOffsetAt(nx)) : -1;
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
  if (kind == power::Session::LowBattery) lowBatterySleep();  // returns only if sleeping failed
  if (earlyClear) present::earlyClear(pill.x, pill.y, pill.w, pill.h);  // no wait: the CPU work below overlaps the waveform
  if (!g_scheduled) input::begin();  // a scheduled refresh never initialises touch input
  // The frame allocations need no bus: do them while the early waveform runs, before the config load takes the SD.
  bool framesReady = earlyClear && present::begin();

  // Cold boot splash (docs/ui/splash.md): power-on, reset button or flashing. One GC16 refresh, then the rest of setup runs.
  bool splashed = false;
  uint32_t splashMs = 0;
  {
    if (!boot::fallbackActive() && sleepwake::splashWanted() && (framesReady || present::begin())) {
      framesReady = true;
      // Reset button and real cold boots show the splash (deep-sleep and power-off wakes keep the image); the app then follows
      // with a full redraw (bootFrame(... || splashed) below).
      render::drawSplash(present::canvas(), "v" FW_VERSION);
      present::setLogTab(g_app.shownTab());
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
  logMem("after_config");
  if (cfg.error) {
    g_app.initError(cfg.errors, s.orientation);
    const auto &e = cfg.errors[0];
    wakelog::logf("BOOT: config_error path=%s line=%d msg=%s\n", e.path.c_str(), e.line, e.message.c_str());
  } else {
    // Demo (fake states, local fake actions) unless the config has a connection section.
    g_demoMode = app_logic::isDemoRun(s, cfg.demo, cfg.cfg.hasConnection);
    g_real = !g_demoMode;
    logMem("before_app_init");
    g_app.init(cfg.cfg, s.orientation, g_demoMode);
    dropCfgTabs();
    logMem("after_app_init");
    if (sleepwake::restoreTabWanted() && g_app.restoreTab(sleepwake::savedTab())) boot::log("tab_restored", (long)g_app.shownTab());  // task 086
    g_app.setActionSink(onAppAction);
  g_app.setBrightnessSink(onBrightnessSend);
  g_app.setMediaSink(onMediaSend);
  g_app.setAlarmSink(onAlarmSend);  // task 108
    g_app.setMarksDriven(g_real);
    boot::log("mode", g_real ? "real" : "demo");
    if (g_real) g_restoredEpoch = state_cache::restore(cfg.cfg.entityIds, g_app.store());
    logMem("after_restore");
    graphsInit();
    logMem("after_graphs");
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

  // Start Wi-Fi early (task 058): the connect overlaps frame composition and the first draw. The net task
  // touches neither the display nor the SD card before the first fetch result, which the loop applies.
  if (g_real) {
    session_glue::loadClockFromRtc();
    if (batteryTooLowForWifi()) lowBatterySleep();  // the brownouts hit when Wi-Fi starts (task 079)
    logMem("before_net_begin");
    g_net.begin(cfg.cfg.connection.ha.internalUrl, cfg.cfg.connection.ha.externalUrl, cfg.cfg.connection.ha.token);
    startSession(millis());
  }
  g_keepStatus = g_real && sleepwake::wokeFromSleep();

  if (!framesReady && !present::begin()) {
    present::settleEarlyClear();
    wakelog::writeDeferred();
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
  present::EarlyClear early{pill.x, pill.y, pill.w, pill.h, [](M5Canvas &c) {
    const int64_t nx = sleepwake::panelNextAt();
    render::drawSleepBadgeOverlay(c, g_settings.orientation, int(sleepwake::panelIndicator()),
                                  nx > 0 ? app_logic::localMinuteOfDay(nx, utcOffsetAt(nx)) : -1);
  }};
  present::setWakePolicy(app_logic::deferRefreshes(g_scheduled),
                         app_logic::firstFrameFull(g_scheduled, present::partialDebt(), s.refresh_interval_min > 0));
  present::setLogTab(g_app.shownTab());  // task 099: g_lastShownTab must be valid for the first tab change
  present::bootFrame(boot::fallbackActive() || splashed, earlyClear ? &early : nullptr);  // after the splash: ghost-free full redraw
  // g_keepStatus stays set: updateStatus() clears it once the session leaves Connecting.
  boot::mark("first_draw");
  wakelog::writeDeferred();
  ota::begin(splashed);  // task 087: state; splashed = cold boot
  ota::setQuiesce(otaQuiesce);
  g_net.setOtaHook(ota::runAutoCheckJob);
  ota::setPoster([]() { if (!g_net.running()) return false; g_net.postOtaCheck(); return true; });
  ota::setYield([]() { return g_net.userWaiting(); });

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

// Diagnostics at the top of every pass: loop stack high-water mark (once) and the debug-build memory tick.
static void loopDiagnostics() {
  static bool stackLogged = false;  // once per session, after the first minute of use
  if (!stackLogged && millis() > 60000) {
    stackLogged = true;
    Serial.printf("BOOT: loop_stack_free=%u\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  }
#ifdef DEBUG_SERIAL
  static uint32_t lastMem = 0;
  if (millis() - lastMem > 30000) {  // every 30 s while awake (debug builds only)
    lastMem = millis();
    logMem("tick");
  }
#endif
#if defined(OTA_TEST_BAD) && defined(DEBUG_SERIAL)
  if (millis() > 4000) { Serial.println("OTA-TEST: bad firmware restarting"); Serial.flush(); boot::endBurstRead(); ESP.restart(); }  // rollback test: never healthy
#endif
}

void loop() {
  loopDiagnostics();
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
  input::Event e;
  if (!loopphase::pollInput(now, e)) return;
  if (!loopphase::keepAwake(now)) return;
  const bool popupOpen = loopphase::popupActive();
  loopphase::sessionInput(now, e, popupOpen);
  loopphase::localIdleSleep(now, e, popupOpen);
  loopphase::dispatchInput(now, e, changed);
  loopphase::netEvents(now, changed);
  loopphase::pumps(changed);
  loopphase::frame(now, e, popupOpen, changed);
}
