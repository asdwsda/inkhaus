#ifdef BOOT_DEBUG_RANGE
#include <M5Unified.h>
#define private public
#include "lgfx/v1/panel/Panel_IT8951.hpp"
#undef private
static void logRange(const char *tag) {
  auto *p = static_cast<lgfx::Panel_IT8951 *>(M5.Display.getPanel());
  Serial.printf("BOOT: range_%s new=[%d,%d,%d,%d]\n", tag, (int)p->_range_new.left, (int)p->_range_new.top,
                (int)p->_range_new.right, (int)p->_range_new.bottom);
}
#else
#include <M5Unified.h>
static void logRange(const char *) {}
#endif
#include <Preferences.h>
#include <esp_ota_ops.h>

#include "boot.h"
#include "screen_track.h"
#include "settings.h"
#include "settings_nvs.h"
#include "version.h"

using screen_track::DirtyTracker;
using screen_track::Rect;

static const char *const kTitle = "M5Paper HA remote";

// Boot diagnostics and the record of what is on the panel live in their own NVS namespace,
// apart from the user settings.
static const char *const kDiagNs = "diag";
static const char *const kKeyBoots = "boots";
static const char *const kKeyScr = "scr";      // screen id of the last full draw
static const char *const kKeyHash = "chash";   // content hash of that draw
static const char *const kKeySig = "psig";     // panel signature read back after that draw

static const uint32_t kScreenBoot = 1;
static const int kBoxW = 240;
static const int kBoxH = 56;

// ---- layout -----------------------------------------------------------------------------------

static int cx() { return M5.Display.width() / 2; }
static int cy() { return M5.Display.height() / 2; }
static Rect counterBox() { return {cx() - kBoxW / 2, cy() + 110, kBoxW, kBoxH}; }

// Everything static on the screen, counter excluded. Changes when text, firmware version,
// rotation or geometry changes.
static uint32_t contentHash(int rotation) {
  uint32_t h = screen_track::fnv1a(kScreenBoot);
  h = screen_track::fnv1a(kTitle, h);
  h = screen_track::fnv1a(FW_VERSION, h);
  h = screen_track::fnv1a((uint32_t)rotation, h);
  h = screen_track::fnv1a((uint32_t)M5.Display.width(), h);
  h = screen_track::fnv1a((uint32_t)M5.Display.height(), h);
  h = screen_track::fnv1a((uint32_t)kBoxW, h);
  h = screen_track::fnv1a((uint32_t)kBoxH, h);
  // Firmware identity: a new build always starts with a full redraw.
  const esp_app_desc_t *app = esp_ota_get_app_description();
  if (app) h = screen_track::fnv1a(app->app_elf_sha256, sizeof(app->app_elf_sha256), h);
  return h;
}

// Hash of a few sample lines through the title and the version, read back from the controller's
// image memory (not from the panel). A different firmware, or a reset that cleared the memory,
// gives a different value.
//
// Every read is bounded: BUSY is checked before each line, and each read burst is ended with
// MEM_BST_END (boot::endBurstRead). Without that the controller stays in the burst after an
// ESP restart and BUSY never releases (task 016, boot.cpp). Returns false if the controller does
// not answer; `out` is then meaningless.
static bool panelSignature(uint32_t &out) {
  auto &d = M5.Display;
  uint8_t line[300 * 3];
  uint32_t h = screen_track::kFnvSeed;
  const int ys[] = {cy() - 52, cy() - 40, cy() - 28, cy() + 20};
  bool ok = true;
  d.startWrite();
  for (int y : ys) {
    if (!boot::waitBusyHigh(300, "sig_line")) { ok = false; break; }
    d.readRectRGB(cx() - 150, y, 300, 1, line);
    h = screen_track::fnv1a(line, sizeof(line), h);
    if (!boot::endBurstRead()) { ok = false; break; }
  }
  d.endWrite();
  boot::feed();
  out = h;
  return ok;
}

// ---- NVS --------------------------------------------------------------------------------------

static screen_track::Record loadRecord() {
  screen_track::Record r;
  Preferences p;
  if (!p.begin(kDiagNs, true)) return r;
  r.valid = p.isKey(kKeyScr) && p.isKey(kKeyHash) && p.isKey(kKeySig);
  if (r.valid) {
    r.screen_id = p.getUInt(kKeyScr, 0);
    r.content_hash = p.getUInt(kKeyHash, 0);
    r.panel_sig = p.getUInt(kKeySig, 0);
  }
  p.end();
  return r;
}

// Forgets the record (a partial key set reads as "no record"). Called before a full draw starts, so
// a power loss mid-draw leads to another full draw next time.
static bool invalidateRecord() {
  Preferences p;
  if (!p.begin(kDiagNs, false)) return false;
  p.remove(kKeyScr);
  p.remove(kKeyHash);
  p.remove(kKeySig);
  const bool gone = !p.isKey(kKeyScr) && !p.isKey(kKeyHash) && !p.isKey(kKeySig);
  p.end();
  return gone;
}

// Writes all three keys and reads them back; returns false if anything did not stick.
static bool saveRecord(const screen_track::Record &r) {
  Preferences p;
  if (!p.begin(kDiagNs, false)) return false;
  const size_t a = p.putUInt(kKeyScr, r.screen_id);
  const size_t b = p.putUInt(kKeyHash, r.content_hash);
  const size_t c = p.putUInt(kKeySig, r.panel_sig);
  const bool ok = a && b && c && p.getUInt(kKeySig, ~r.panel_sig) == r.panel_sig;
  p.end();
  return ok;
}

// Increments and returns the boot counter (this boot's number, starting at 1).
static uint32_t nextBootNumber() {
  Preferences p;
  if (!p.begin(kDiagNs, false)) return 1;
  const uint32_t n = p.getUInt(kKeyBoots, 0) + 1;
  p.putUInt(kKeyBoots, n);
  p.end();
  return n;
}

// ---- drawing ----------------------------------------------------------------------------------

// Counter box content with pure black/white text: the text is rendered anti-aliased into a gray
// sprite and thresholded, because epd_fastest only does black/white (task 015: cleaner than
// anti-aliased text there).
static void drawCounterContent(uint32_t boots) {
  auto &d = M5.Display;
  const Rect b = counterBox();
  char buf[24];
  snprintf(buf, sizeof(buf), "boots: %u", (unsigned)boots);

  const int iw = b.w - 2, ih = b.h - 2;
  lgfx::LGFX_Sprite spr(&d);
  spr.setColorDepth(lgfx::grayscale_8bit);
  const bool sprOk = spr.createSprite(iw, ih) != nullptr;
  if (sprOk) {
    spr.fillSprite(TFT_WHITE);
    spr.setTextColor(TFT_BLACK, TFT_WHITE);
    spr.setTextDatum(middle_center);
    spr.setFont(&fonts::DejaVu24);
    spr.drawString(buf, iw / 2, ih / 2);
  }
  d.startWrite();
  d.fillRect(b.x, b.y, b.w, b.h, TFT_WHITE);
  d.drawRect(b.x, b.y, b.w, b.h, TFT_BLACK);
  if (sprOk) {
    // Threshold in RAM (fast), then one block transfer to the panel.
    for (int y = 0; y < ih; ++y) {
      for (int x = 0; x < iw; ++x) {
        spr.drawPixel(x, y, spr.readPixelValue(x, y) < 128 ? TFT_BLACK : TFT_WHITE);
      }
    }
    spr.pushSprite(&d, b.x + 1, b.y + 1);
  }
  d.endWrite();
}

static void drawStaticScreen() {
  auto &d = M5.Display;
  d.startWrite();
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::DejaVu40);
  d.drawString(kTitle, cx(), cy() - 40);
  d.setFont(&fonts::DejaVu24);
  d.drawString("v" FW_VERSION, cx(), cy() + 20);
  d.endWrite();
}

static void refresh(epd_mode_t mode, const Rect *r) {
  auto &d = M5.Display;
  d.setEpdMode(mode);
  const uint32_t t0 = micros();
  if (r) d.display(r->x, r->y, r->w, r->h);
  else d.display();
  const uint32_t t1 = micros();
  d.waitDisplay();  // bounded: Panel_IT8951::_check_afsr gives up after 3 s
  boot::feed();
  Serial.printf("BOOT: refresh_mode=%d call_ms=%u wait_ms=%u\n", (int)mode, (unsigned)((t1 - t0) / 1000),
                (unsigned)((micros() - t1) / 1000));
}

// One clean pass over every region that was updated with epd_fastest: white fill refreshed with
// epd_quality, then the content again with epd_fastest (task 015, box D).
static void cleanupRegions(const DirtyTracker &tracker, uint32_t boots) {
  auto &d = M5.Display;
  for (size_t i = 0; i < tracker.size(); ++i) {
    const Rect r = tracker.at(i);
    d.setEpdMode(epd_mode_t::epd_quality);
    d.startWrite();
    d.fillRect(r.x, r.y, r.w, r.h, TFT_WHITE);
    d.endWrite();
    refresh(epd_mode_t::epd_quality, &r);
    d.setEpdMode(epd_mode_t::epd_fastest);
    drawCounterContent(boots);  // the only tracked region today is the counter box
    refresh(epd_mode_t::epd_fastest, &r);
  }
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
  boot::mark("settings");

  const int rotation = (s.orientation == settings::Orientation::Landscape) ? 1 : 0;
  auto &d = M5.Display;
  // Panel_IT8951 sets _auto_display=true: every outermost endWrite() runs display(0,0,0,0), i.e.
  // refreshes the accumulated dirty range at once (Panel.hpp:88, Panel_IT8951.inl:116). That made
  // the re-render a full-screen refresh. Turn it off; all refreshes here are explicit.
  d.getPanel()->setAutoDisplay(false);
  d.setRotation(rotation);

  const uint32_t boots = nextBootNumber();
  const uint32_t chash = contentHash(rotation);
  const screen_track::Record stored = loadRecord();
  const uint32_t t0 = micros();
  uint32_t sigNow = 0;
  bool sigOk = boot::ensureControllerReady("before_sig") && panelSignature(sigNow);
  if (!sigOk) {
    // The controller did not answer in time; reset it (cold) instead of hanging.
    boot::ensureControllerReady("sig_failed");
    sigNow = 0;
  }
  boot::log("sig_ms", (long)((micros() - t0) / 1000));
  boot::log("sig_now", (long)sigNow);
  boot::log("sig_stored", stored.valid ? (long)stored.panel_sig : -1L);
  // A power-cycled controller has an empty image memory, so the signature says nothing then.
  const bool sigComparable = sigOk && !boot::controllerWasCold();
  boot::log("sig_match", (stored.valid && stored.panel_sig == sigNow) ? 1L : 0L);
  auto decision = screen_track::decide(stored, kScreenBoot, chash, sigNow, sigComparable);
  if (boot::fallbackActive() && !decision.full) {
    // After repeated watchdog resets nothing is known about the controller state: redraw.
    decision.full = true;
    decision.reason = screen_track::Reason::PanelMismatch;
  }

  boot::log("boots", (long)boots);
  boot::log("draw", decision.full ? "full" : "partial");
  if (decision.full) {
    boot::log("reason", screen_track::reasonName(decision.reason));
    boot::log("reason_detail", screen_track::reasonDetail(decision.reason));
  }

#ifdef BOOT_TEST_HANG
  // Forced hang for the watchdog test: 1 = skipped once the fallback is active, 2 = always.
  if (BOOT_TEST_HANG >= 2 || !boot::fallbackActive()) {
    boot::log("test_hang", "blocking");
    while (true) delay(100);
  }
#endif
  DirtyTracker tracker;
  if (decision.full) {
    // Unknown picture on the panel: one full quality redraw, nothing partial over it.
    // Strongest clearing waveform the panel driver exposes: GC16 (epd_quality), which drives every
    // pixel. A white GC16 pass first wipes the old picture, then the content is drawn with GC16
    // again. (UPDATE_MODE_INIT is private in Panel_IT8951.)
    // The picture is about to change: forget the old record first, so that a power loss during
    // the draw leads to another full draw (the new record is written after it finished).
    boot::log("record_invalidated", invalidateRecord() ? 1L : 0L);
    boot::log("full_mode", "gc16_white_then_gc16");
    d.startWrite();
    d.setEpdMode(epd_mode_t::epd_quality);
    d.fillScreen(TFT_WHITE);
    d.endWrite();
    refresh(epd_mode_t::epd_quality, nullptr);
    drawStaticScreen();
    drawCounterContent(boots);
    refresh(epd_mode_t::epd_quality, nullptr);
    screen_track::Record rec;
    rec.valid = true;
    rec.screen_id = kScreenBoot;
    rec.content_hash = chash;
    uint32_t sigAfter = 0;
    if (panelSignature(sigAfter)) {
      rec.panel_sig = sigAfter;
      if (saveRecord(rec)) boot::log("sig_saved", (long)sigAfter);
      else boot::log("sig_saved", "skipped_nvs_write_failed");
    } else {
      // stays invalid: next boot draws fully
      boot::log("sig_saved", "skipped_readback_failed");
    }
  } else {
    // The panel shows our screen. If the controller's image memory is not known to hold it (it
    // was power-cycled, or the signature differs), re-render all of it into that memory first,
    // then drop the pending refresh range that this drawing built up, so that only the counter
    // box is refreshed. With auto-display off the drawing only grows the panel's dirty range
    // (_range_new); Panel::init(false) resets that range (Panel_IT8951.inl:132-135) and leaves
    // the image memory alone (no reset pulse, no clear).
    const uint32_t r0 = micros();
    const bool memoryKnown = sigComparable && stored.panel_sig == sigNow;
    if (!memoryKnown) {
      drawStaticScreen();
      logRange("after_render");
      d.getPanel()->init(false);
      logRange("after_init");
    }
    const uint32_t r1 = micros();
    const uint32_t r2 = r1;
    const Rect box = counterBox();
    drawCounterContent(boots);
    tracker.mark(box);
    logRange("after_counter");
    const uint32_t r3 = micros();
    refresh(epd_mode_t::epd_fastest, &box);
    const uint32_t r4 = micros();
    boot::log("rerender", memoryKnown ? "no" : "yes");
    boot::log("rerender_ms", (long)((r1 - r0) / 1000));
    boot::log("counter_draw_ms", (long)((r3 - r2) / 1000));
    boot::log("counter_refresh_ms", (long)((r4 - r3) / 1000));
  }
  boot::mark("first_draw");

  cleanupRegions(tracker, boots);
  boot::log("cleanup_regions", (long)tracker.size());
  boot::mark("cleanup");

  boot::log("version", FW_VERSION);
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
#ifdef BOOT_TEST_RESTART
  static bool done = false;
  if (!done) { done = true; testRestart(); }
#endif
  delay(1000);
}
