#pragma once
// Pushes the composed UI frame to the e-ink panel: diff against the last pushed frame, refresh
// only what changed (fast mode, tracked), clean up after a quiet period, full-quality refresh for
// tab/page changes, and the persisted record that makes a cold boot refresh only what differs.
// Every log line is "PRESENT: key=value".
#include <M5Unified.h>
#include <stdint.h>

#include <vector>

#include "screen_track.h"
#include "burst_clean.h"

namespace present {

// Allocates the current and previous frame (8-bit grey, PSRAM) at the display's current size.
bool begin();
// False after a failed begin(): no frame exists; canvas()/previous() must not be used.
bool framesOk();
// Minimal error on the panel without a canvas (GC16 refresh, DejaVu24, centred).
void showError(const char *msg);
// Full-screen notice (task 024, low battery): a big title and a hint line, GC16, centred, ASCII only.
// Forgets the stored screen record (the panel no longer shows the app) and forces the next frame to
// be a full redraw. Works without frames.
void showNotice(const char *title, const char *hint);
// Identifies the running firmware build. Call before anything that reads or writes the screen record:
// a record written by another build is treated as missing (full redraw, splash on a cold boot).
void setFirmwareId(uint32_t id);
// True if the stored screen record is valid for the current geometry and firmware (needs the rotation set).
bool hasValidRecord();
// The frame to draw into.
M5Canvas &canvas();
const M5Canvas &previous();

// Task 058, wake feedback: clears the sleep badge's pill on the panel at once. Fills the 4-px-aligned
// rect (x, y, w, h, display coordinates, rotation already set) paper and starts an epd_quality refresh of
// just that rect; does NOT wait (the controller is cold, its previous image unknown: no epd_fastest).
// Needs no frames. Nothing may touch the controller or the SD card until bootFrame() or settleEarlyClear().
// Logs t_wake_feedback_ms.
void earlyClear(int x, int y, int w, int h);
// Waits for the early refresh (no-op if none is running); logs t_wake_feedback_done_ms.
void settleEarlyClear();
// What bootFrame() needs to know about an early clear: the cleared rect and a function drawing the badge
// into a canvas. The rect counts as paper in the diff baseline when the new frame has paper there.
struct EarlyClear {
  int x, y, w, h;
  void (*drawBadge)(M5Canvas &);
};

// First frame after boot: `canvas()` holds the rendered screen. Decides between a full redraw
// (no record, other geometry, forced) and "diff against the stored tile hashes of what the panel
// shows", pushes the whole frame into controller memory without a refresh in the latter case.
void bootFrame(bool forceFull, const EarlyClear *early = nullptr);

// A new frame is in canvas(). Full: GC16 refresh of the whole screen (tab/page change).
// Partial: diff against the previous frame; per changed rectangle, epd_fastest if the panel holds only
// black/white there, else one epd_quality refresh. Logs "rect=... mode=... reason=..." per rect.
// Returns true if anything was refreshed.
enum class Mode { Partial, Full };
bool show(Mode mode, uint32_t now);
// Modal popup (history, brightness) open/closed, call before every show(). Opening: the next show() refreshes
// the popup rect once in quality with the real greys (no big-change full refresh); while open, quality
// refreshes keep greys. Closing: the next show() is one full GC16 refresh of the whole screen.
void setModal(bool open);
// Task 074: rects (display coordinates) refreshed as one when any change lands inside (graph widgets).
// Call before every show(); an empty list turns it off.
void setMergeRegions(const std::vector<screen_track::Rect> &regions);
// Task 107: rects (display coordinates) holding 16-grey album art. A changed rect that touches one is refreshed with
// one epd_quality (GC16) pass pushing the real greys; the rest of the frame keeps the fast path. Call before show().
void setGreyRegions(const std::vector<screen_track::Rect> &regions);
// Task 120 (architect 2026-11-05): fastest refreshes whose rect lies inside one of these regions (the status bar
// indicator) add to the ghost-debt area only: not to the partial count, not to the burst count. Call before show().
void setIndicatorRegions(const std::vector<screen_track::Rect> &regions);
// Task 123: the next show() (Partial only) refreshes just the changed rects that touch `r` (display coordinates), e.g.
// the keypad code box after a key press; other changes stay unpushed and are caught by a later show(). Call before show().
void limitNextShow(const screen_track::Rect &r);
void setLogTab(int tab);  // tab index shown in the "APP: render" log line; also how show() detects a tab change
// Task 099: tab-change cleanup variant 0..3 (lib/screen_track/src/tab_clean.h). 0 = GC16 full (default, unchanged);
// the debug build's `tabclean N` serial command changes it. Applies only to a Full frame whose tab differs from
// the last shown one; not stored.
void setTabCleanVariant(int v);
int tabCleanVariant();

// Task 114: burst clean. Defaults n = 4 partials, idle 2 s, mode gc16 (device look decides). n < 0 turns it off;
// mode = gc16 | gl16 | init+gl16 (nullptr keeps it). Debug serial: `burstclean N T mode`.
void setBurstClean(int n, uint32_t idleMs, const char *mode);
// True when more than n fast partials ran since the last full and the user was idle >= T (idleMs = time since the
// last input); false in the given context exclusions (popup, Settings, OTA, pending tap, scheduled session).
bool burstCleanDue(uint32_t idleMs, const screen_track::BurstContext &ctx);
// One full refresh of the whole frame (mode as configured); logs `PRESENT: refresh=full_burst partials=N mode=..`.
void burstClean();

// Call regularly: after 2 s without a frame, runs one GC16 full refresh if the ghost debt is
// exceeded (30 fastest refreshes or 300 % screen area since the last full refresh) and saves the
// record. Returns true if it did something.
bool idle(uint32_t now);
// Call right before sleeping: GC16 full refresh if the debt is at least half the limit.
bool prepareSleep();
// Task 092 (grey drift). Call before bootFrame. deferred (scheduled wake): bootFrame and show() draw nothing and
// only note whether the frame changed. firstFrameFull: bootFrame does one GC16 full when partial debt is set.
void setWakePolicy(bool deferred, bool firstFrameFull);
// Task 101: the sleep screen is the frame on the panel (NVS, survives sleep). Set by the caller after it drew the
// sleep screen; bootFrame() of an interactive wake then replaces it by one GC16 full and clears the flag.
bool sleepScreenOnPanel();
void setSleepScreenOnPanel(bool on);
// Task 095: one GC16 full of the current frame now (log `full_idle`), clears the debt. Call only when idle.
void idleFull();
bool deferChanged();   // the deferred frames differ from the panel
bool partialDebt();    // partial refreshes since the last GC16 full (NVS, kept across sleep)
// End of a deferred session: the frame in canvas() (sleep badge included) as one GC16 full if `full`
// (app_logic::scheduledSleepFull), else nothing. Returns true if it refreshed. No-op if not deferred.
bool finishScheduled(bool full);
// Number of GC16 full refreshes in this run (0 after every wake).
uint32_t fullCount();

}  // namespace present
