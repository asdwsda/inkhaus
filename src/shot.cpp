#include "shot.h"

#include <esp_heap_caps.h>
#include <string.h>

#include <string>

#include "boot.h"
#include "frame_diff.h"
#include "provision.h"

namespace shot {

namespace {

std::string g_line;

void dump(const M5Canvas &c, bool rle) {
  const int w = c.width(), h = c.height();
  const uint8_t *px = static_cast<const uint8_t *>(const_cast<M5Canvas &>(c).getBuffer());
  const size_t n = size_t(w) * size_t(h);
  if (!rle) {
    Serial.printf("SHOT-BEGIN %d %d\n", w, h);
    Serial.printf("P5\n%d %d\n255\n", w, h);
    size_t off = 0;
    while (off < n) {
      const size_t chunk = n - off > 4096 ? 4096 : n - off;
      Serial.write(px + off, chunk);
      off += chunk;
    }
    Serial.print("\nSHOT-END\n");
    return;
  }
  uint8_t *out = (uint8_t *)malloc(n);  // worst case 2n; screens compress far better
  size_t len = 0;
  if (out) len = screen_track::rleEncode(px, n, out, n);
  if (!out || len == 0) {
    // Does not fit: fall back to the raw dump.
    free(out);
    dump(c, false);
    return;
  }
  Serial.printf("SHOT-BEGIN %d %d rle %u\n", w, h, (unsigned)len);
  size_t off = 0;
  while (off < len) {
    const size_t chunk = len - off > 4096 ? 4096 : len - off;
    Serial.write(out + off, chunk);
    off += chunk;
  }
  Serial.print("\nSHOT-END\n");
  free(out);
}

// Dumps the IT8951 image memory of a screen rect. The raw controller value is what the panel's next
// fast refresh is computed against. Always ends the read burst.
void dumpMem(int x, int y, int w, int h) {
  auto &d = M5.Display;
  if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > d.width() || y + h > d.height()) {
    Serial.print("MEM-ERR bad rect\n");
    return;
  }
  const size_t n = size_t(w) * size_t(h);
  uint8_t *rgb = (uint8_t *)heap_caps_malloc(n * 3, MALLOC_CAP_SPIRAM);
  uint8_t *gray = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  uint8_t *out = (uint8_t *)heap_caps_malloc(n * 2, MALLOC_CAP_SPIRAM);
  if (!rgb || !gray || !out) {
    Serial.print("MEM-ERR alloc\n");
    heap_caps_free(rgb);
    heap_caps_free(gray);
    heap_caps_free(out);
    return;
  }
  // waitDisplay()'s register read leaves the controller in a read state; without this the first
  // memory read after a refresh stalls on BUSY for minutes.
  boot::endBurstRead();
  d.readRectRGB(x, y, w, h, rgb);
  const bool ended = boot::endBurstRead();
  for (size_t i = 0; i < n; ++i) gray[i] = rgb[i * 3];
  const size_t len = screen_track::rleEncode(gray, n, out, n * 2);
  Serial.printf("MEM-BEGIN %d %d %d %d rle %u\n", x, y, w, h, (unsigned)len);
  size_t off = 0;
  while (off < len) {
    const size_t chunk = len - off > 4096 ? 4096 : len - off;
    Serial.write(out + off, chunk);
    off += chunk;
  }
  Serial.printf("\nMEM-END bst_end=%d\n", ended ? 1 : 0);
  heap_caps_free(rgb);
  heap_caps_free(gray);
  heap_caps_free(out);
}

bool handle(const std::string &line, const Hooks &hk) {
  if (provision::handleLine(line)) return true;  // putfile / reboot (blocks while receiving)
  if (line == "shot" || line == "shotz") {
    if (hk.frame) dump(*hk.frame, line == "shotz");
    return true;
  }
  if (line == "shotp") {
    if (hk.previous) dump(*hk.previous, true);
    return true;
  }
  int x, y, w, h;
  unsigned ms;
  if (sscanf(line.c_str(), "mem %d %d %d %d", &x, &y, &w, &h) == 4) {
    if (hk.frame) dumpMem(x, y, w, h);
    return true;
  }
  if (sscanf(line.c_str(), "cleanup %u", &ms) == 1) {
    Serial.printf("CMD: cleanup %u (no-op, clean-up pass removed)\n", ms);
    return true;
  }
  if (sscanf(line.c_str(), "tap %d %d", &x, &y) == 2) {
    input::Event e;
    e.kind = input::Event::Touch;
    e.point = {x, y};
    input::inject(e);
    Serial.printf("CMD: tap %d %d\n", x, y);
    return true;
  }
  if (line.rfind("key ", 0) == 0) {
    input::Event e;
    e.kind = input::Event::Key;
    const std::string k = line.substr(4);
    if (k == "up") e.key = ui::PhysicalKey::Up;
    else if (k == "down") e.key = ui::PhysicalKey::Down;
    else if (k == "push") e.key = ui::PhysicalKey::Push;
    else if (k == "long") e.key = ui::PhysicalKey::LongPush;
    else return false;
    input::inject(e);
    Serial.printf("CMD: key %s\n", k.c_str());
    return true;
  }
  if (sscanf(line.c_str(), "pend %u", &ms) == 1) {
    if (hk.setPendingMs) hk.setPendingMs(ms);
    Serial.printf("CMD: pend %u\n", ms);
    return true;
  }
  return false;
}

}  // namespace

bool poll(const Hooks &hooks) {
  bool did = false;
  while (Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (!g_line.empty()) did |= handle(g_line, hooks);
      g_line.clear();
    } else if (g_line.size() < 64) {
      g_line += char(ch);
    }
  }
  return did;
}

}  // namespace shot
