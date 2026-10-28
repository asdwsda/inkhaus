#pragma once
// Pushes the composed UI frame to the e-ink panel: diff against the last pushed frame, refresh
// only what changed (fast mode, tracked), clean up after a quiet period, full-quality refresh for
// tab/page changes, and the persisted record that makes a cold boot refresh only what differs.
// Every log line is "PRESENT: key=value".
#include <M5Unified.h>
#include <stdint.h>

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
// The frame to draw into.
M5Canvas &canvas();
const M5Canvas &previous();

// First frame after boot: `canvas()` holds the rendered screen. Decides between a full redraw
// (no record, other geometry, forced) and "diff against the stored tile hashes of what the panel
// shows", pushes the whole frame into controller memory without a refresh in the latter case.
void bootFrame(bool forceFull);

// A new frame is in canvas(). Full: GC16 refresh of the whole screen (tab/page change).
// Partial: diff against the previous frame; per changed rectangle, epd_fastest if the panel holds only
// black/white there, else one epd_quality refresh. Logs "rect=... mode=... reason=..." per rect.
// Returns true if anything was refreshed.
enum class Mode { Partial, Full };
bool show(Mode mode, uint32_t now);

// Call regularly: after 2 s without a frame, runs one GC16 full refresh if the ghost debt is
// exceeded (30 fastest refreshes or 300 % screen area since the last full refresh) and saves the
// record. Returns true if it did something.
bool idle(uint32_t now);
// Call right before sleeping: GC16 full refresh if the debt is at least half the limit.
bool prepareSleep();

}  // namespace present
