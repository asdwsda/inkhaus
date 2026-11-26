#pragma once
// Touch and rocker input as abstract events (task 017). Hardware wrapper over M5Unified.
#include "ui_layout.h"
#include "ui_rocker.h"

namespace input {

struct Event {
  enum Kind { None, Touch, LongTouch, TouchRelease, Key, SleepNow } kind = None;
  ui::Point point;                              // Touch/LongTouch/TouchRelease: panel coordinates in the current rotation
  ui::PhysicalKey key = ui::PhysicalKey::Push;  // Key
};

// Call once after M5.begin().
void begin();
// Calls M5.update() and returns the next event (None if there is none). Touch acts on press;
// LongTouch: the finger is still down after 600 ms (point = where it went down). TouchRelease: the finger
// was lifted (point = last position). A tap on a dimmable light is deferred until one of the two (task 045).
// BtnA = up, BtnC = down, BtnB click = push, BtnB held 600 ms..2.5 s and released = long push,
// BtnB held 2.5 s = SleepNow (task 081; fires at 2.5 s, no long push follows).
Event poll();
// Task 126 D1: M5.update() + queue the new events, without popping. Called from the refresh wait loop so that a
// touch that starts and ends inside a blocking EPD wait is not lost. UNVERIFIED on device.
void sample();
// Number of buffered events not yet returned by poll().
int pending();
// Drops all buffered events (end of setup: the waking press must not fire later).
void clear();
// Injects an event (serial test commands) as if it came from the hardware.
void inject(const Event &e);

}  // namespace input
