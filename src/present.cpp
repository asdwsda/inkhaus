#include <algorithm>
#include "present.h"

#include <Preferences.h>
#include <esp_heap_caps.h>

#include <vector>

#include "boot.h"
#include "frame_diff.h"
#include "modal_settle.h"
#include "screen_track.h"

using screen_track::Rect;

namespace present {

namespace {

M5Canvas g_cur(&M5.Display);
M5Canvas g_prev(&M5.Display);   // the last composed frame (reference for the diff)
M5Canvas g_panel(&M5.Display);  // exactly what was pushed to the controller (thresholded or not)
int g_w = 0, g_h = 0;

constexpr uint32_t kQuietMs = 2000;       // no frame for this long: idle work (ghost-debt full refresh)
constexpr uint32_t kScreenApp = 0x41505000u;  // + rotation
// Ghost debt = fastest refreshes since the last full refresh. Fastest over pure black/white is
// clean (user-judged), and quality refreshes add no debt, so the limits are generous. Untuned:
// 30 frames, or fastest area summing to 300 % of the screen, triggers one GC16 full refresh at
// the next idle moment. Before sleep the bar is half of that (prepareSleep).
constexpr int kGhostPartials = 30;
constexpr int kGhostAreaPct = 300;
// Pushing the unthresholded image in a quality refresh leaves anti-aliased greys in the panel,
// which would force every later update of that rect to quality again. false: the quality refresh
// pushes the thresholded image (GC16 shows 0/255 exactly), so the rect is black/white afterwards.
constexpr bool kQualityPushesGrey = false;
constexpr int kBigChangePct = 50;          // one frame changing more than this: refresh it as a page

uint32_t g_lastShowMs = 0;
bool g_havePending = false;     // fastest refreshes since the last full refresh (ghost debt)
bool g_recordValid = false;     // the NVS record still describes the panel
bool g_recordDirty = false;     // panel content changed since the record was written
bool g_forceFull = false;       // a refresh failed: next frame is a full redraw
bool g_readOpen = false;        // a refresh wait left the controller in the read state (needs endBurstRead)
bool g_framesOk = false;
int g_partialCount = 0;
long g_partialArea = 0;

const char *kNs = "diag";
const char *kKeyScr = "scr", *kKeyHash = "chash", *kKeySig = "psig", *kKeyTiles = "tiles";

uint32_t g_fwId = 0;  // identifies the running firmware build; a record written by another one is not trusted

uint32_t screenId() { return kScreenApp + (uint32_t)M5.Display.getRotation(); }

// ---- record in NVS ---------------------------------------------------------------------------

bool recordKeysGone(Preferences &p) {
  return !p.isKey(kKeyScr) && !p.isKey(kKeyHash) && !p.isKey(kKeySig) && !p.isKey(kKeyTiles);
}

// Forgets the record before the panel content changes. Escalates if a plain remove does not stick:
// a stale valid record must never survive a draw (task 016 carry-over).
bool invalidateRecord() {
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  p.remove(kKeyHash);
  p.remove(kKeyScr);
  p.remove(kKeySig);
  p.remove(kKeyTiles);
  bool gone = recordKeysGone(p);
  if (!gone) {
    boot::log("record_invalidate", "retry_clear");
    p.clear();
    gone = recordKeysGone(p);
  }
  if (!gone) {
    // Last resort: make the stored hash unmatchable so that the record reads as invalid.
    boot::log("record_invalidate", "retry_poison");
    p.putUInt(kKeyHash, 0xDEADBEEFu);
    gone = p.getUInt(kKeyHash, 0) == 0xDEADBEEFu;
  }
  p.end();
  g_recordValid = false;
  return gone;
}

bool saveRecord(const std::vector<uint16_t> &tiles) {
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  const uint32_t hash = screen_track::hashOfTiles(tiles);
  const size_t a = p.putBytes(kKeyTiles, tiles.data(), tiles.size() * sizeof(uint16_t));
  const size_t b = p.putUInt(kKeyScr, screenId());
  const size_t c = p.putUInt(kKeySig, g_fwId);
  const size_t d = p.putUInt(kKeyHash, hash);  // last: the record only counts once the hash is in
  const bool ok = a == tiles.size() * sizeof(uint16_t) && b && c && d && p.getUInt(kKeyHash, ~hash) == hash;
  p.end();
  return ok;
}

// Stored tile hashes if the record is valid for this geometry.
bool loadRecord(std::vector<uint16_t> &tiles) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  bool ok = false;
  if (p.isKey(kKeyScr) && p.isKey(kKeyHash) && p.isKey(kKeyTiles) && p.getUInt(kKeyScr, 0) == screenId() &&
      p.getUInt(kKeySig, 0) == g_fwId) {
    const size_t want = screen_track::tileGridFor(g_w, g_h).count();
    const size_t have = p.getBytesLength(kKeyTiles);
    if (have == want * sizeof(uint16_t)) {
      tiles.resize(want);
      p.getBytes(kKeyTiles, tiles.data(), have);
      ok = screen_track::hashOfTiles(tiles) == p.getUInt(kKeyHash, 0);
    }
  }
  p.end();
  return ok;
}

// ---- panel access ----------------------------------------------------------------------------

uint8_t *buf(M5Canvas &c) { return static_cast<uint8_t *>(c.getBuffer()); }

void waitAndCheck(const char *what) {
  auto &d = M5.Display;
  d.waitDisplay();
  g_readOpen = true;  // the register read in waitDisplay never sends MEM_BST_END
  boot::feed();
  if (d.displayBusy()) {
    // waitDisplay gave up (the library bounds it at 3 s): log and recover; the controller memory
    // can no longer be trusted, so the next frame is a full redraw.
    boot::log("refresh_timeout", what);
    boot::ensureControllerReady("refresh");
    g_forceFull = true;
  }
}

void refresh(epd_mode_t mode, const Rect *r, const char *what) {
  auto &d = M5.Display;
  d.setEpdMode(mode);
  const uint32_t t0 = micros();
  if (r) d.display(r->x, r->y, r->w, r->h);
  else d.display();
  const uint32_t t1 = micros();
  waitAndCheck(what);
  Serial.printf("PRESENT: refresh=%s mode=%d rect=%d,%d,%d,%d call_ms=%u wait_ms=%u\n", what, (int)mode,
                r ? r->x : 0, r ? r->y : 0, r ? r->w : g_w, r ? r->h : g_h,
                (unsigned)((t1 - t0) / 1000), (unsigned)((micros() - t1) / 1000));
}

// Writes a region of the current frame into the controller's memory. `bw`: thresholded black and
// white (what epd_fastest shows; the panel would otherwise dither the greys).
void pushRegion(const Rect &r, bool bw) {
  auto &d = M5.Display;
  d.startWrite();
  if (bw) {
    uint8_t *tmp = (uint8_t *)heap_caps_malloc(size_t(r.w) * size_t(r.h), MALLOC_CAP_SPIRAM);
    if (tmp) {
      const uint8_t *src = buf(g_cur);
      for (int y = 0; y < r.h; ++y) {
        const uint8_t *s = src + size_t(r.y + y) * size_t(g_w) + size_t(r.x);
        uint8_t *o = tmp + size_t(y) * size_t(r.w);
        for (int x = 0; x < r.w; ++x) o[x] = s[x] < 160 ? 0 : 255;
      }
      d.pushGrayscaleImage(r.x, r.y, r.w, r.h, tmp, lgfx::grayscale_8bit, 0xFFFFFFu, 0x000000u);
      uint8_t *pn = buf(g_panel);
      for (int y = 0; y < r.h; ++y)
        memcpy(pn + size_t(r.y + y) * size_t(g_w) + size_t(r.x), tmp + size_t(y) * size_t(r.w), size_t(r.w));
      heap_caps_free(tmp);
      d.endWrite();
      return;
    }
  }
  d.setClipRect(r.x, r.y, r.w, r.h);
  g_cur.pushSprite(&d, 0, 0);
  d.clearClipRect();
  d.endWrite();
  const uint8_t *src = buf(g_cur);
  uint8_t *pn = buf(g_panel);
  for (int y = 0; y < r.h; ++y)
    memcpy(pn + size_t(r.y + y) * size_t(g_w) + size_t(r.x), src + size_t(y + r.y) * size_t(g_w) + size_t(r.x),
           size_t(r.w));
}

void copyToPrev(const Rect &r) {
  const uint8_t *s = buf(g_cur);
  uint8_t *p = buf(g_prev);
  for (int y = 0; y < r.h; ++y) memcpy(p + size_t(r.y + y) * g_w + r.x, s + size_t(r.y + y) * g_w + r.x, size_t(r.w));
}

void copyAllToPrev() {
  memcpy(buf(g_prev), buf(g_cur), size_t(g_w) * size_t(g_h));
  memcpy(buf(g_panel), buf(g_cur), size_t(g_w) * size_t(g_h));
}

// True if the panel content (as pushed) has any grey (neither pure black nor pure white) pixel in `r`.
bool panelHasGrey(const Rect &r) {
  const uint8_t *src = buf(g_panel);
  for (int y = r.y; y < r.y + r.h; ++y) {
    const uint8_t *row = src + size_t(y) * size_t(g_w);
    for (int x = r.x; x < r.x + r.w; ++x)
      if (row[x] != 0 && row[x] != 255) return true;
  }
  return false;
}

Rect whole() { return {0, 0, g_w, g_h}; }

void saveRecordOfCurrent() {
  const auto tiles = screen_track::tileHashes(buf(g_cur), g_w, g_h, g_w);
  const bool ok = saveRecord(tiles);
  g_recordValid = ok;
  g_recordDirty = !ok;
  boot::log("record_saved", ok ? 1L : 0L);
}

// The panel content is about to change by a fast refresh: the stored record stops describing it.
void touchRecord() {
  if (g_recordValid) {
    const bool ok = invalidateRecord();
    boot::log("record_invalidated", ok ? 1L : 0L);
  }
  g_recordDirty = true;
}

// Full-quality refresh of the whole frame (GC16). `whiteFirst`: wipe an unknown picture first.
void fullRefresh(bool whiteFirst, const char *why) {
  auto &d = M5.Display;
  g_forceFull = false;  // before the refresh: a timeout inside it sets the flag again
  if (g_recordValid) touchRecord();
  if (whiteFirst) {
    d.startWrite();
    d.setEpdMode(epd_mode_t::epd_quality);
    d.fillScreen(TFT_WHITE);
    d.endWrite();
    refresh(epd_mode_t::epd_quality, nullptr, "full_white");
  }
  d.setEpdMode(epd_mode_t::epd_quality);
  pushRegion(whole(), false);
  refresh(epd_mode_t::epd_quality, nullptr, why);
  copyAllToPrev();
  g_partialCount = 0;
  g_partialArea = 0;
  g_havePending = false;
}


// Refreshes one changed rectangle. Fastest only if the panel's content there (what was pushed
// last) is pure black/white; otherwise one quality refresh. Keeps g_panel and g_prev exact.
bool g_modal = false;       // setModal(): a popup is open; its rect keeps real greys
bool g_modalOpening = false;  // the next show() is the opening one
bool g_modalClosing = false;  // the next show() is the closing one
screen_track::ModalSettle g_settle(1000);  // popup fastest updates: one quality refresh ~1 s after the last

void refreshRect(const Rect &r, const char *tag, bool unknownPanel) {
  auto &d = M5.Display;
  const bool grey = unknownPanel || g_modalOpening || panelHasGrey(r);
  const char *reason = unknownPanel ? "boot_unknown_panel" : g_modalOpening ? "modal" : (grey ? "panel_has_grey" : "panel_bw");
  Serial.printf("PRESENT: rect=%d,%d,%d,%d mode=%s reason=%s\n", r.x, r.y, r.w, r.h, grey ? "quality" : "fastest",
                reason);
  if (grey) {
    d.setEpdMode(epd_mode_t::epd_quality);
    pushRegion(r, !(kQualityPushesGrey || g_modal));  // popup: real greys, g_panel records them
    refresh(epd_mode_t::epd_quality, &r, tag);
  } else {
    d.setEpdMode(epd_mode_t::epd_fastest);
    pushRegion(r, true);
    refresh(epd_mode_t::epd_fastest, &r, tag);
    if (g_modal) {
      g_settle.noteFastest(r, millis());  // popup debt: cleared by the settle refresh, not by a full one
    } else {
      ++g_partialCount;
      g_partialArea += long(r.w) * long(r.h);
      g_havePending = true;
    }
  }
  copyToPrev(r);
}

void closeRead() {
  if (!g_readOpen) return;
  g_readOpen = false;
  boot::log("present_bst_end", boot::endBurstRead() ? 1L : 0L);
}

}  // namespace

bool begin() {
  auto &d = M5.Display;
  // Re-begin (orientation change): free the old canvases first, or the new ones need 3 more frames.
  boot::log("psram_free_before", (long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  g_cur.deleteSprite();
  g_prev.deleteSprite();
  g_panel.deleteSprite();
  g_w = d.width();
  g_h = d.height();
  g_cur.setColorDepth(lgfx::grayscale_8bit);
  g_prev.setColorDepth(lgfx::grayscale_8bit);
  g_cur.setPsram(true);
  g_prev.setPsram(true);
  g_panel.setColorDepth(lgfx::grayscale_8bit);
  g_panel.setPsram(true);
#ifdef TEST_ALLOC_FAIL
  const bool ok = g_cur.createSprite(g_w, g_h) && false;  // test build: pretend the next allocation fails
#else
  const bool ok = g_cur.createSprite(g_w, g_h) && g_prev.createSprite(g_w, g_h) && g_panel.createSprite(g_w, g_h);
#endif
  boot::log("frame_alloc", ok ? 1L : 0L);
  g_framesOk = ok;
  if (!ok) {
    g_cur.deleteSprite();
    g_prev.deleteSprite();
    g_panel.deleteSprite();
  } else {
    g_cur.fillScreen(TFT_WHITE);
    g_prev.fillScreen(TFT_WHITE);
    g_panel.fillScreen(TFT_WHITE);
  }
  boot::log("psram_free", (long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));  // after
  return ok;
}

void showError(const char *msg) {
  auto &d = M5.Display;
  boot::log("error_screen", msg);
  d.startWrite();
  d.setEpdMode(epd_mode_t::epd_quality);
  d.fillScreen(TFT_WHITE);
  d.setFont(&fonts::DejaVu24);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(textdatum_t::middle_center);
  d.drawString(msg, d.width() / 2, d.height() / 2);
  d.endWrite();
  d.display();
  waitAndCheck("error");
  closeRead();
}

void showNotice(const char *title, const char *hint) {
  auto &d = M5.Display;
  boot::log("notice_screen", title);
  const bool inv = invalidateRecord();
  boot::log("record_invalidated", inv ? 1L : 0L);
  g_recordValid = false;
  g_havePending = false;
  g_forceFull = true;
  d.startWrite();
  d.setEpdMode(epd_mode_t::epd_quality);
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(textdatum_t::middle_center);
  d.setFont(&fonts::DejaVu40);
  d.drawString(title, d.width() / 2, d.height() / 2 - 30);
  d.setFont(&fonts::DejaVu24);
  d.drawString(hint, d.width() / 2, d.height() / 2 + 30);
  d.endWrite();
  d.display();
  waitAndCheck("notice");
  closeRead();
}

bool framesOk() { return g_framesOk; }

void setFirmwareId(uint32_t id) { g_fwId = id; }

bool hasValidRecord() {
  std::vector<uint16_t> tiles;
  return loadRecord(tiles);
}

M5Canvas &canvas() { return g_cur; }
const M5Canvas &previous() { return g_panel; }

void bootFrame(bool forceFull) {
  auto &d = M5.Display;
  std::vector<uint16_t> stored;
  const bool have = !forceFull && loadRecord(stored);
  boot::log("record", have ? "valid" : (forceFull ? "ignored_forced" : "none_or_other_geometry"));
  const uint32_t t0 = micros();
  if (!have) {
    // Unknown picture on the panel: forget the record first (a power loss mid-draw then leads to
    // another full draw), then GC16 white and GC16 content.
    const bool inv = invalidateRecord();
    boot::log("record_invalidated", inv ? 1L : 0L);
    fullRefresh(true, "full_content");
    if (inv) saveRecordOfCurrent();
    else boot::log("record_saved", "skipped_invalidate_failed");  // never leave a stale record behind
    g_lastShowMs = millis();
    boot::log("draw", "full");
    closeRead();
    return;
  }
  g_recordValid = true;
  const auto now = screen_track::tileHashes(buf(g_cur), g_w, g_h, g_w);
  const auto rects = screen_track::diffTiles(stored, now, g_w, g_h);
  // The controller's memory may hold garbage (cold start) or be intact; either way, write the
  // whole frame into it without refreshing, then refresh only what differs from the panel.
  const uint32_t t1 = micros();
  d.setEpdMode(epd_mode_t::epd_quality);
  pushRegion(whole(), false);
  // The write grew the panel's pending refresh range to the whole screen; drop it (task 014:
  // Panel::init(false) resets the range and leaves the image memory alone).
  d.getPanel()->init(false);
  boot::log("rerender_ms", (long)((micros() - t1) / 1000));
  copyAllToPrev();
  boot::log("diff_rects", (long)rects.size());
  if (!rects.empty()) touchRecord();
  // Task 058: smallest rect first. The sleep indicator (the thing the user is waiting to see go away
  // after a wake) is far smaller than the status text; its refresh is done first. Not merged into one
  // union: the union of two distant rects is much larger and not clearly faster.
  std::vector<Rect> order(rects.begin(), rects.end());
  std::stable_sort(order.begin(), order.end(), [](const Rect &a, const Rect &b) { return a.w * a.h < b.w * b.h; });
  bool firstRect = true;
  for (const Rect &r : order) {
    Serial.printf("PRESENT: boot_rect=%d,%d,%d,%d\n", r.x, r.y, r.w, r.h);
    refreshRect(r, "boot_diff", true);  // what the panel shows there is only known by hash: quality
    if (firstRect) boot::log("t_boot_diff1_done_ms", (long)boot::elapsedMs());
    firstRect = false;
  }
  g_lastShowMs = millis();
  boot::log("draw", rects.empty() ? "none" : "partial");
  boot::log("boot_present_ms", (long)((micros() - t0) / 1000));
  closeRead();
}

static int g_renderTab = -1;
void setLogTab(int tab) { g_renderTab = tab; }

void setModal(bool open) {
  if (open == g_modal) return;
  g_modal = open;
  g_settle.reset();
  if (open) g_modalOpening = true;
  else g_modalClosing = true;
}

bool show(Mode mode, uint32_t now) {
  auto &d = M5.Display;
  g_lastShowMs = now;
  const bool exempt = g_modalOpening;
  const bool closing = g_modalClosing;
  struct Clear { ~Clear() { g_modalOpening = false; g_modalClosing = false; } } clearExempt;
  // Popup close: one full GC16 refresh of the whole screen (greys pushed, ghost debt reset).
  if (mode == Mode::Full || g_forceFull || closing) {
    if (mode != Mode::Full) boot::log("full_after_failure", 1);
    Serial.printf("APP: render tab=%d rects=1 mode=full\n", g_renderTab);
    fullRefresh(g_forceFull, "full");
    saveRecordOfCurrent();
    return true;
  }
  screen_track::DiffOptions opts;
  auto rects = screen_track::diffFrames(buf(g_prev), buf(g_cur), g_w, g_h, g_w, opts);
  if (rects.empty()) return false;
  Serial.printf("APP: render tab=%d rects=%u mode=partial\n", g_renderTab, (unsigned)rects.size());
  long area = 0;
  for (const Rect &r : rects) area += long(r.w) * long(r.h);
  const long screen = long(g_w) * long(g_h);
  if (!exempt && area * 100 > screen * kBigChangePct) {
    boot::log("big_change_pct", area * 100 / screen);
    Serial.printf("APP: render tab=%d rects=%u mode=full\n", g_renderTab, (unsigned)rects.size());
    fullRefresh(false, "full_big_change");
    saveRecordOfCurrent();
    return true;
  }
  touchRecord();
  for (const Rect &r : rects) refreshRect(r, "partial", false);
  return true;
}

bool ghostDebt(int partials, int areaPct) {
  return g_partialCount >= partials || g_partialArea * 100 > long(g_w) * long(g_h) * areaPct;
}

// Popup ghost clean-up: one quality refresh of the popup rect, real greys (g_modal), debt reset.
static bool settleModal(uint32_t now) {
  if (!g_modal || !g_settle.due(now)) return false;
  const Rect r = g_settle.rect();
  g_settle.done();
  Serial.printf("PRESENT: modal_settle rect=%d,%d,%d,%d\n", r.x, r.y, r.w, r.h);
  auto &d = M5.Display;
  touchRecord();
  d.setEpdMode(epd_mode_t::epd_quality);
  pushRegion(r, false);
  refresh(epd_mode_t::epd_quality, &r, "modal_settle");
  copyToPrev(r);
  g_lastShowMs = millis();
  return true;
}

bool idle(uint32_t now) {
  // Close the read state after every refresh, whichever path it took (a Full show leaves nothing
  // pending, so the early return below would skip it): a restart can come at any moment.
  closeRead();
  if (g_modal) {  // no ghost-debt full refresh under a popup; the close does a full one
    const bool did = settleModal(now);
    if (did) closeRead();
    return did;
  }
  if (!g_havePending && !g_recordDirty) return false;
  if (now - g_lastShowMs < kQuietMs) return false;
  const uint32_t t0 = micros();
  if (g_havePending && ghostDebt(kGhostPartials, kGhostAreaPct)) {
    boot::log("ghost_debt", (long)g_partialCount);
    fullRefresh(false, "full_ghost");
    boot::log("ghost_full_ms", (long)((micros() - t0) / 1000));
  }
  if (g_recordDirty) saveRecordOfCurrent();
  closeRead();
  boot::endBurstRead();
  g_lastShowMs = millis();
  return true;
}

bool prepareSleep() {
  bool ran = false;
  if (g_havePending && ghostDebt(kGhostPartials / 2, kGhostAreaPct / 2)) {
    boot::log("ghost_debt_sleep", (long)g_partialCount);
    fullRefresh(false, "full_before_sleep");
    ran = true;
  }
  if (g_recordDirty || ran) saveRecordOfCurrent();
  g_readOpen = false;
  boot::endBurstRead();
  return ran;
}

}  // namespace present
