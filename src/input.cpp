#include "input.h"

#include <M5Unified.h>

namespace input {

namespace {
constexpr int kQueue = 8;
Event g_queue[kQueue];
int g_head = 0, g_count = 0;

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
}

void inject(const Event &e) { push(e); }

Event poll() {
  M5.update();
  if (M5.BtnA.wasPressed()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Up; push(e); }
  if (M5.BtnC.wasPressed()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Down; push(e); }
  if (M5.BtnB.wasHold()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::LongPush; push(e); }
  else if (M5.BtnB.wasClicked()) { Event e; e.kind = Event::Key; e.key = ui::PhysicalKey::Push; push(e); }
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
  if (g_count == 0) return Event();
  Event e = g_queue[g_head];
  g_head = (g_head + 1) % kQueue;
  --g_count;
  return e;
}

}  // namespace input
