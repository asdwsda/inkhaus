#include "input.h"

#include <M5Unified.h>

namespace input {

namespace {
constexpr int kQueue = 16;  // task 126 D1: taps sampled during refresh waits are buffered
Event g_queue[kQueue];
int g_head = 0, g_count = 0;
ui::PushHold g_hold;
bool g_begun = false;  // sample() is a no-op before begin() (scheduled wakes never call it)

void push(const Event &e) {
  if (g_count == kQueue) return;
  g_queue[(g_head + g_count) % kQueue] = e;
  ++g_count;
}
}  // namespace

// UNVERIFIED: M5.Touch hold_begin fires once after 600 ms on the GT911 and a release gives wasReleased().
void begin() {
  M5.BtnB.setHoldThresh(600);
  M5.Touch.setHoldThresh(600);
  g_begun = true;
}

void clear() { g_head = g_count = 0; }

void inject(const Event &e) { push(e); }

void sample() {
  if (!g_begun) return;
  M5.update();
  if (M5.BtnA.wasPressed()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Up; push(e); }
  if (M5.BtnC.wasPressed()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Down; push(e); }
  switch (g_hold.update(millis(), M5.BtnB.isPressed())) {
    case ui::PushHold::Out::LongPush: { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::LongPush; push(e); break; }
    case ui::PushHold::Out::SleepNow: { Event e; e.kind = Event::SleepNow; push(e); break; }
    case ui::PushHold::Out::None: break;
  }
  if (M5.BtnB.wasClicked()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Push; push(e); }
  const auto t = M5.Touch.getDetail();
  if (t.wasPressed()) {
    Event e;
    e.kind = Event::Touch;
    e.point = {t.x, t.y};
    push(e);
  } else if (t.wasHold()) {
    Event e;
    e.kind = Event::LongTouch;
    e.point = {t.x, t.y};
    push(e);
  }
  if (t.wasReleased()) {
    Event e;
    e.kind = Event::TouchRelease;
    e.point = {t.x, t.y};
    push(e);
  }
}

int pending() { return g_count; }

Event poll() {
  sample();
  if (g_count == 0) return Event();
  Event e = g_queue[g_head];
  g_head = (g_head + 1) % kQueue;
  --g_count;
  return e;
}

}  // namespace input
