#include <M5Unified.h>

#include "app_config.h"
#include "app_state.h"
#include "boot.h"
#include "input.h"
#include "present.h"
#include "render/screen.h"
#include "settings.h"
#include "settings_nvs.h"
#include "shot.h"

// Test-only build flags (never in the normal firmware):
//   -DDEMO_LANDSCAPE       start in landscape instead of the stored orientation
//   -DTEST_CONFIG_BROKEN   inject a broken config (error screen)
//   -DTEST_CONFIG_MOCKUP   inject the approved mockup's config, mock status label, frozen pending
//   -DBOOT_TEST_HANG=n / -DBOOT_TEST_RESTART  task 016 watchdog and warm-restart tests

static uistate::AppState g_app;
static render::StatusInfo g_status;

static int batteryPercent() {
#ifdef TEST_CONFIG_MOCKUP
  return 78;
#else
  const int v = M5.Power.getBatteryLevel();
  return (v < 0 || v > 100) ? 100 : v;
#endif
}

static void composeFrame() {
  g_status.batteryPct = batteryPercent();
  render::renderScreen(present::canvas(), g_app, g_status);
}

static void showFrame(uint32_t now) {
  composeFrame();
  const bool full = g_app.takeFull();
  present::show(full ? present::Mode::Full : present::Mode::Partial, now);
}

void setup() {
  boot::begin();

  settings::Settings s;
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

  app_config::Loaded cfg = app_config::load();
  boot::log("config_source", cfg.source);
  if (cfg.error) {
    g_app.initError(cfg.errors, s.orientation);
    const auto &e = cfg.errors[0];
    Serial.printf("BOOT: config_error path=%s line=%d msg=%s\n", e.path.c_str(), e.line, e.message.c_str());
  } else {
    g_app.init(cfg.cfg, s.orientation, cfg.demo);
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
  if (e.kind == input::Event::Touch) changed |= g_app.touch(e.point, now);
  else if (e.kind == input::Event::Key) changed |= g_app.rocker(e.key, now);

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
