// Grey-drift stress test (task 092). Standalone, awake on USB.
// Phase A: GC16 full of the test image, then N partial refreshes of one box with DU4 (epd_fastest).
// Phase B: GC16 full, N partial refreshes with GC16 (epd_quality).
// Phase C: GC16 full, same duration as A/B, no refreshes (time control).
// Each phase starts on a rocker press (any direction/push) so the user can photograph in between.
// Serial: "DRIFT: key=value".
#include <M5Unified.h>

namespace {
constexpr int kN = 100;               // partial refreshes per phase
constexpr uint32_t kPeriodMs = 2000;  // start-to-start; every phase lasts kN * kPeriodMs = 200 s
constexpr int kBx = 40, kBy = 380, kBw = 160, kBh = 120;  // toggled box (white bg around)

constexpr int kPinUp = 37, kPinPush = 38, kPinDown = 39;
bool anyDown() { return !digitalRead(kPinUp) || !digitalRead(kPinPush) || !digitalRead(kPinDown); }

void waitPress(const char *what) {
  Serial.printf("DRIFT: waiting_for_rocker=%s\n", what);
  while (anyDown()) delay(10);
  delay(50);
  while (!anyDown()) delay(10);
  while (anyDown()) delay(10);
  delay(100);
}

void drawImage(const char *title) {
  auto &g = M5.Display;
  g.setEpdMode(epd_mode_t::epd_quality);
  g.fillScreen(0xFFFFFFu);
  g.setTextColor(0x000000u, 0xFFFFFFu);
  g.setFont(&fonts::DejaVu40);
  g.setCursor(20, 10);
  g.print(title);
  g.setFont(&fonts::DejaVu24);
  g.setCursor(20, 70);
  g.print("Grey drift test: background must stay white.");
  // 16 grey steps
  for (int i = 0; i < 16; i++) {
    const uint8_t v = i * 17;
    g.fillRect(20 + i * 56, 120, 56, 90, g.color888(v, v, v));
  }
  // checker (pure b/w)
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 16; x++)
      if ((x + y) & 1) g.fillRect(20 + x * 56, 230 + y * 30, 56, 30, 0x000000u);
  // outline around the toggled box area
  g.drawRect(kBx - 6, kBy - 6, kBw + 12, kBh + 12, 0x000000u);
  g.setCursor(260, 400);
  g.print("white area right of the box = drift reference");
}

void box(int i) {
  auto &g = M5.Display;
  g.fillRect(kBx, kBy, kBw, kBh, (i & 1) ? 0xFFFFFFu : 0x000000u);
  g.setFont(&fonts::DejaVu24);
  g.setTextColor((i & 1) ? 0x000000u : 0xFFFFFFu, (i & 1) ? 0xFFFFFFu : 0x000000u);
  g.setCursor(kBx + 20, kBy + 45);
  g.printf("%03d", i);
}

void full(const char *title) {
  drawImage(title);
  const uint32_t t = millis();
  M5.Display.display();
  M5.Display.waitDisplay();
  Serial.printf("DRIFT: full_gc16_ms=%lu\n", (unsigned long)(millis() - t));
}

void phase(char id, epd_mode_t mode, bool refresh) {
  char title[48];
  snprintf(title, sizeof title, "Phase %c: %s", id, refresh ? (mode == epd_mode_t::epd_fastest ? "100x DU4 partial" : "100x GC16 partial") : "idle (no refresh)");
  full(title);
  Serial.printf("DRIFT: phase_%c_start ms=%lu\n", id, (unsigned long)millis());
  const uint32_t t0 = millis();
  uint32_t sumMs = 0, maxMs = 0;
  int n = 0;
  for (int i = 0; i < kN; i++) {
    const uint32_t due = t0 + (uint32_t)i * kPeriodMs;
    while ((int32_t)(millis() - due) < 0) delay(5);
    if (!refresh) continue;
    auto &g = M5.Display;
    g.setEpdMode(mode);
    box(i);
    const uint32_t t = millis();
    g.display(kBx, kBy, kBw, kBh);
    g.waitDisplay();
    const uint32_t d = millis() - t;
    sumMs += d;
    if (d > maxMs) maxMs = d;
    n++;
  }
  while ((int32_t)(millis() - (t0 + (uint32_t)kN * kPeriodMs)) < 0) delay(5);
  Serial.printf("DRIFT: phase_%c_end refreshes=%d avg_ms=%lu max_ms=%lu total_s=%lu\n", id, n,
                n ? (unsigned long)(sumMs / n) : 0UL, (unsigned long)maxMs,
                (unsigned long)((millis() - t0) / 1000));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kPinUp, INPUT);
  pinMode(kPinPush, INPUT);
  pinMode(kPinDown, INPUT);
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  cfg.clear_display = false;
  cfg.output_power = false;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.external_imu = false;
  cfg.external_rtc = false;
  cfg.led_brightness = 0;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setAutoDisplay(false);

  drawImage("DRIFT TEST - press rocker to start A");
  M5.Display.display();
  M5.Display.waitDisplay();
  waitPress("start_A");
  phase('A', epd_mode_t::epd_fastest, true);
  waitPress("photo_A_then_start_B");
  phase('B', epd_mode_t::epd_quality, true);
  waitPress("photo_B_then_start_C");
  phase('C', epd_mode_t::epd_quality, false);
  Serial.println("DRIFT: all_done");
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
}

void loop() { delay(1000); }
