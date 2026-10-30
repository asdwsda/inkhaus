#pragma once
// Pushes the composed UI frame to the e-ink panel: diff against the last pushed frame, refresh
// only what changed (fast mode, tracked), clean up after a quiet period, full-quality refresh for
// tab/page changes, and the persisted record that makes a cold boot refresh only what differs.
// Every log line is "PRESENT: key=value".
#include <M5Unified.h>
#include <stdint.h>

#include <vector>

#include "screen_track.h"

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
void setLogTab(int tab);  // tab index shown in the "APP: render" log line

// Call regularly: after 2 s without a frame, runs one GC16 full refresh if the ghost debt is
// exceeded (30 fastest refreshes or 300 % screen area since the last full refresh) and saves the
// record. Returns true if it did something.
bool idle(uint32_t now);
// Call right before sleeping: GC16 full refresh if the debt is at least half the limit.
bool prepareSleep();

}  // namespace present
