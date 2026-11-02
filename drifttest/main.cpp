// Grey-drift stress test v2 (task 092). Standalone, awake on USB.
// Phase A: 150 cycles, each toggles 8 small zones spread over the screen with DU4 (epd_fastest).
// Phase B: same with GC16 (epd_quality).
// Phase D: 150 IT8951 power cycles (SLEEP 0x0003, then hardware reset + init as the epdoff wake path), no pixel change.
//   The TPS65185 is not reachable from here (M5GFX has no PMIC access); the IT8951 powers the panel rails itself
//   on each display(), which phases A/B exercise.
// Every phase: GC16 full, test, then GC16 redraw of the RIGHT half only ("fresh half"; left = "test half"),
// then waits for a rocker press. Serial: "DRIFT: key=value", "all_done" at the end.
#include <M5Unified.h>
#include "lgfx/v1/panel/Panel_IT8951.hpp"

namespace {
constexpr int kCycles = 150;
struct Zone { int x, y; };
constexpr int kZw = 60, kZh = 40;
constexpr Zone kZones[] = {{880, 10}, {600, 10}, {10, 490}, {890, 490}, {450, 440}, {200, 150}, {700, 280}, {250, 440}};
constexpr int kNZones = sizeof(kZones) / sizeof(kZones[0]);

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
  for (int i = 0; i < 16; i++) {
    const uint8_t v = i * 17;
    g.fillRect(20 + i * 56, 120, 56, 90, g.color888(v, v, v));
  }
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 16; x++)
      if ((x + y) & 1) g.fillRect(20 + x * 56, 230 + y * 30, 56, 30, 0x000000u);
  g.setFont(&fonts::DejaVu40);
  g.setCursor(20, 380);
  g.print("test half");
  g.setCursor(500, 380);
  g.print("fresh half");
  g.drawFastVLine(480, 0, 540, 0x000000u);
}

void full(const char *title) {
  drawImage(title);
  const uint32_t t = millis();
  M5.Display.display();
  M5.Display.waitDisplay();
  Serial.printf("DRIFT: full_gc16_ms=%lu\n", (unsigned long)(millis() - t));
}

// GC16 redraw of the right half only (fresh reference next to the drifted left half).
void freshRightHalf(const char *title) {
  auto &g = M5.Display;
  g.setClipRect(480, 0, 480, 540);
  drawImage(title);
  g.clearClipRect();
  const uint32_t t = millis();
  g.display(480, 0, 480, 540);
  g.waitDisplay();
  Serial.printf("DRIFT: fresh_half_gc16_ms=%lu\n", (unsigned long)(millis() - t));
}

void zonePhase(char id, epd_mode_t mode, const char *title) {
  full(title);
  Serial.printf("DRIFT: phase_%c_start cycles=%d zones=%d ms=%lu\n", id, kCycles, kNZones, (unsigned long)millis());
  auto &g = M5.Display;
  const uint32_t t0 = millis();
  uint32_t sumMs = 0, maxMs = 0, n = 0;
  for (int c = 0; c < kCycles; c++) {
    for (const Zone &z : kZones) {
      g.setEpdMode(mode);
      g.fillRect(z.x, z.y, kZw, kZh, (c & 1) ? 0xFFFFFFu : 0x000000u);
      const uint32_t t = millis();
      g.display(z.x, z.y, kZw, kZh);
      g.waitDisplay();
      const uint32_t d = millis() - t;
      sumMs += d;
      if (d > maxMs) maxMs = d;
      n++;
    }
    if (c % 25 == 24) Serial.printf("DRIFT: phase_%c_progress cycle=%d\n", id, c + 1);
  }
  Serial.printf("DRIFT: phase_%c_end refreshes=%lu avg_ms=%lu max_ms=%lu total_s=%lu\n", id, (unsigned long)n,
                (unsigned long)(n ? sumMs / n : 0), (unsigned long)maxMs, (unsigned long)((millis() - t0) / 1000));
}

void powerPhase(char id, const char *title) {
  full(title);
  Serial.printf("DRIFT: phase_%c_start cycles=%d ms=%lu\n", id, kCycles, (unsigned long)millis());
  auto &g = M5.Display;
  auto *p = static_cast<lgfx::Panel_IT8951 *>(g.getPanel());
  const uint32_t t0 = millis();
  uint32_t sumMs = 0, maxMs = 0;
  for (int c = 0; c < kCycles; c++) {
    const uint32_t t = millis();
    p->setSleep(true);  // SLEEP 0x0003
    delay(300);
    p->init(true);      // hardware reset + init (controller image memory becomes garbage; panel untouched)
    g.setRotation(1);
    g.setAutoDisplay(false);
    const uint32_t d = millis() - t;
    sumMs += d;
    if (d > maxMs) maxMs = d;
    if (c % 25 == 24) Serial.printf("DRIFT: phase_%c_progress cycle=%d\n", id, c + 1);
  }
  Serial.printf("DRIFT: phase_%c_end power_cycles=%d avg_ms=%lu max_ms=%lu total_s=%lu\n", id, kCycles,
                (unsigned long)(sumMs / kCycles), (unsigned long)maxMs, (unsigned long)((millis() - t0) / 1000));
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

  drawImage("DRIFT TEST v2 - press rocker to start A");
  M5.Display.display();
  M5.Display.waitDisplay();
  waitPress("start_A");
  zonePhase('A', epd_mode_t::epd_fastest, "Phase A: DU4 zones");
  freshRightHalf("Phase A: DU4 zones");
  waitPress("photo_A_then_start_B");
  zonePhase('B', epd_mode_t::epd_quality, "Phase B: GC16 zones");
  freshRightHalf("Phase B: GC16 zones");
  waitPress("photo_B_then_start_D");
  powerPhase('D', "Phase D: power cycles");
  freshRightHalf("Phase D: power cycles");
  waitPress("photo_D");
  Serial.println("DRIFT: all_done");
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
}

void loop() { delay(1000); }
