// Hardware probe firmware (task 013). Separate env `probe`; not part of the app.
// Every result is one serial line `PROBE: <key>=<value>` and is shown on screen.
//
// Input rules:
//  - A short press is decided on release (so it cannot be confused with a long one).
//  - A long press (M5Unified hold, 500 ms) of any rocker button (A/B/C) skips the
//    current step and logs `PROBE: <step>=skipped`. Any of A/B/C is accepted because
//    the push button mapping is not known before step 4.
//  - After every accepted input there is a 300 ms lockout to swallow bounce.
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>

#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------- logging

struct SummaryItem {
  std::string key;
  std::string value;
};
std::vector<SummaryItem> g_summary;

void kv(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void kv(const char *key, const char *fmt, ...) {
  char val[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(val, sizeof(val), fmt, ap);
  va_end(ap);
  Serial.printf("PROBE: %s=%s\n", key, val);
}

// Same as kv, and remember it for the final summary screen.
void kvs(const char *key, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void kvs(const char *key, const char *fmt, ...) {
  char val[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(val, sizeof(val), fmt, ap);
  va_end(ap);
  Serial.printf("PROBE: %s=%s\n", key, val);
  g_summary.push_back({key, val});
}

// ---------------------------------------------------------------- screen

void prompt(const char *line1, const char *line2 = nullptr) {
  auto &d = M5.Display;
  d.setEpdMode(epd_mode_t::epd_fast);
  d.setRotation(0);
  d.startWrite();
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::DejaVu40);
  d.drawString(line1, d.width() / 2, d.height() / 2 - (line2 ? 30 : 0));
  if (line2) d.drawString(line2, d.width() / 2, d.height() / 2 + 30);
  d.setFont(&fonts::DejaVu24);
  d.drawString("hold a rocker button = skip step", d.width() / 2, d.height() - 30);
  d.endWrite();
  d.display();
  d.waitDisplay();
}

// ---------------------------------------------------------------- input

enum class Kind { None, Short, Long, Timeout };
struct Input {
  Kind kind = Kind::None;
  int button = -1;  // 0=A 1=B 2=C 3=PWR
};

const char *const kBtnName[4] = {"A", "B", "C", "PWR"};
uint32_t g_lockoutUntil = 0;

m5::Button_Class *btn(int i) {
  switch (i) {
    case 0: return &M5.BtnA;
    case 1: return &M5.BtnB;
    case 2: return &M5.BtnC;
    default: return &M5.BtnPWR;
  }
}

// Update inputs, log every button event, return the first accepted rocker/PWR input.
// Outside the lockout window only. Events are always logged.
Input poll() {
  M5.update();
  Input in;
  for (int i = 0; i < 4; i++) {
    auto *b = btn(i);
    if (b->wasPressed()) kv("btn", "%s event=pressed", kBtnName[i]);
    if (b->wasHold()) kv("btn", "%s event=long", kBtnName[i]);
    if (b->wasReleased()) kv("btn", "%s event=released", kBtnName[i]);
    if (in.kind != Kind::None) continue;
    const bool locked = (int32_t)(millis() - g_lockoutUntil) < 0;
    if (locked) continue;
    if (b->wasHold()) {
      in.kind = Kind::Long;
      in.button = i;
    } else if (b->wasReleased() && !b->wasReleasedAfterHold()) {
      in.kind = Kind::Short;
      in.button = i;
    }
  }
  if (in.kind != Kind::None) g_lockoutUntil = millis() + 300;
  return in;
}

// Wait for a short or long press. timeoutMs == 0: wait forever.
Input waitInput(uint32_t timeoutMs = 0) {
  g_lockoutUntil = millis() + 300;  // swallow the release of the previous press
  const uint32_t start = millis();
  for (;;) {
    Input in = poll();
    if (in.kind != Kind::None) return in;
    if (timeoutMs && millis() - start >= timeoutMs) {
      in.kind = Kind::Timeout;
      return in;
    }
    delay(5);
  }
}

// ---------------------------------------------------------------- steps

const char *boardName(m5gfx::board_t b) {
  switch (b) {
    case m5gfx::board_t::board_unknown: return "unknown";
    case m5gfx::board_t::board_M5Paper: return "M5Paper";
    case m5gfx::board_t::board_M5PaperS3: return "M5PaperS3";
    default: return "other";
  }
}

const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";
    case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

void stepFacts() {
  const auto board = M5.getBoard();
  kvs("board", "%d (%s)", (int)board, boardName(board));
  kvs("psram_bytes", "%u", (unsigned)ESP.getPsramSize());
  kv("heap_free", "%u", (unsigned)ESP.getFreeHeap());
  kv("flash_bytes", "%u", (unsigned)ESP.getFlashChipSize());
  kv("cpu_mhz", "%u", (unsigned)ESP.getCpuFreqMHz());
  kv("reset_reason", "%s", resetReasonName(esp_reset_reason()));

  kvs("battery_mv", "%d", (int)M5.Power.getBatteryVoltage());
  kv("battery_level", "%d", (int)M5.Power.getBatteryLevel());
  switch (M5.Power.isCharging()) {
    case m5::Power_Class::is_charging: kv("charging", "yes"); break;
    case m5::Power_Class::is_discharging: kv("charging", "no"); break;
    default: kv("charging", "unsupported"); break;
  }

  if (M5.Rtc.isEnabled()) {
    m5::rtc_date_t date;
    m5::rtc_time_t time;
    M5.Rtc.getDateTime(&date, &time);
    kv("rtc_datetime", "%04d-%02d-%02d %02d:%02d:%02d", (int)date.year, (int)date.month, (int)date.date,
       (int)time.hours, (int)time.minutes, (int)time.seconds);
    // BM8563 VL flag: set when the clock lost power / was never set.
    const bool volt_low = M5.Rtc.getVoltLow();
    kv("rtc_volt_low", "%s", volt_low ? "yes" : "no");
    kv("rtc_ever_set", "%s", (volt_low || date.year < 2024) ? "no" : "yes");
  } else {
    kv("rtc_datetime", "unsupported");
    kv("rtc_ever_set", "unsupported");
  }

  // M5Unified has no SHT30 API; reading it over raw I2C is out of scope.
  kv("sht30", "unsupported");

  M5.Display.setRotation(0);
  kv("display_rot0", "%dx%d", (int)M5.Display.width(), (int)M5.Display.height());
  M5.Display.setRotation(1);
  kv("display_rot1", "%dx%d", (int)M5.Display.width(), (int)M5.Display.height());
  M5.Display.setRotation(0);
}

// Returns false if skipped (never for the gate; skip just proceeds).
void stepGate() {
  prompt("Hardware probe", "push the rocker to start");
  waitInput();
}

int g_pushButton = -1;

void stepRocker() {
  struct Item {
    const char *prompt;
    const char *key;
  } items[3] = {{"Push the rocker UP", "rocker_up"},
                {"Push the rocker DOWN", "rocker_down"},
                {"PUSH IN the rocker", "rocker_push"}};
  for (int i = 0; i < 3; i++) {
    prompt(items[i].prompt);
    Input in = waitInput();
    if (in.kind == Kind::Long) {
      kvs(items[i].key, "skipped");
      continue;
    }
    kvs(items[i].key, "%s", kBtnName[in.button]);
    if (i == 2) g_pushButton = in.button;
  }

  prompt("Press the side button", "15 s timeout");
  Input in = waitInput(15000);
  if (in.kind == Kind::Long && in.button >= 0 && in.button <= 2) {
    // Long rocker press = skip request.
    kvs("power_btn", "skipped");
  } else if (in.kind == Kind::Timeout) {
    kvs("power_btn", "none");
  } else {
    kvs("power_btn", "%s", kBtnName[in.button]);
  }
}

bool touchStep(int rotation) {
  auto &d = M5.Display;
  char stepKey[24];
  snprintf(stepKey, sizeof(stepKey), "touch_rot%d", rotation);

  d.setEpdMode(epd_mode_t::epd_fast);
  d.setRotation(rotation);
  const int w = d.width();
  const int h = d.height();
  kv("touch_screen", "rot%d %dx%d", rotation, w, h);
  const int m = 40;
  const int pts[5][2] = {{m, m}, {w - m, m}, {w - m, h - m}, {m, h - m}, {w / 2, h / 2}};
  float maxErr = 0;
  for (int i = 0; i < 5; i++) {
    d.startWrite();
    d.fillScreen(TFT_WHITE);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.setTextDatum(middle_center);
    d.setFont(&fonts::DejaVu40);
    char t[32];
    snprintf(t, sizeof(t), "Tap the circle %d/5", i + 1);
    d.drawString(t, w / 2, h / 2 - 110);
    d.fillCircle(pts[i][0], pts[i][1], 24, TFT_BLACK);
    d.fillCircle(pts[i][0], pts[i][1], 8, TFT_WHITE);
    d.endWrite();
    d.display();
    d.waitDisplay();

    // Wait for the finger to be lifted first, then for a fresh touch.
    g_lockoutUntil = millis() + 300;
    for (;;) {
      Input in = poll();
      if (in.kind == Kind::Long) {
        kv(stepKey, "skipped");
        d.setRotation(0);
        return false;
      }
      if (M5.Touch.getCount() > 0) {
        auto t0 = M5.Touch.getDetail(0);
        if (t0.wasPressed() && (int32_t)(millis() - g_lockoutUntil) >= 0) {
          const float dx = t0.x - pts[i][0];
          const float dy = t0.y - pts[i][1];
          const float err = sqrtf(dx * dx + dy * dy);
          if (err > maxErr) maxErr = err;
          kv("touch", "rot%d p%d target=%d,%d got=%d,%d err=%.1f", rotation, i, pts[i][0], pts[i][1], (int)t0.x,
             (int)t0.y, err);
          g_lockoutUntil = millis() + 300;
          break;
        }
      }
      delay(5);
    }
  }
  kvs((std::string("touch_rot") + (rotation ? "1" : "0") + "_max_err").c_str(), "%.0f", maxErr);
  d.setRotation(0);
  return true;
}

void stepTouch() {
  prompt("Touch test", "landscape, then portrait");
  waitInput(1500);  // short info delay; any press (incl. skip) just continues
  if (!touchStep(0)) kvs("touch_rot0", "skipped");
  if (!touchStep(1)) kvs("touch_rot1", "skipped");
}

void drawBars(bool reversed) {
  auto &d = M5.Display;
  const int bw = d.width() / 16;
  for (int i = 0; i < 16; i++) {
    const int g = (reversed ? 15 - i : i) * 17;
    d.fillRect(i * bw, 0, bw, d.height(), d.color888(g, g, g));
  }
}

uint32_t timedRefresh(int x, int y, int w, int h) {
  auto &d = M5.Display;
  const uint32_t t0 = millis();
  if (w > 0) {
    d.display(x, y, w, h);
  } else {
    d.display();
  }
  d.waitDisplay();
  return millis() - t0;
}

void stepEpd() {
  auto &d = M5.Display;
  d.setRotation(0);

  d.setEpdMode(epd_mode_t::epd_quality);
  d.startWrite();
  drawBars(false);
  d.endWrite();
  kvs("epd_quality_full_ms", "%u", (unsigned)timedRefresh(0, 0, 0, 0));

  const int rx = 330, ry = 190, rw = 300, rh = 160;
  struct M {
    epd_mode_t mode;
    const char *key;
    const char *label;
  } modes[4] = {{epd_mode_t::epd_quality, "epd_quality_ms", "quality"},
                {epd_mode_t::epd_fastest, "epd_fastest_ms", "fastest"},
                {epd_mode_t::epd_fast, "epd_fast_ms", "fast"},
                {epd_mode_t::epd_text, "epd_text_ms", "text"}};
  for (int i = 0; i < 4; i++) {
    d.setEpdMode(modes[i].mode);
    d.startWrite();
    d.fillRect(rx, ry, rw, rh, (i & 1) ? TFT_BLACK : TFT_WHITE);
    d.setTextColor((i & 1) ? TFT_WHITE : TFT_BLACK, (i & 1) ? TFT_BLACK : TFT_WHITE);
    d.setTextDatum(middle_center);
    d.setFont(&fonts::DejaVu40);
    d.drawString(modes[i].label, rx + rw / 2, ry + rh / 2);
    d.endWrite();
    kvs(modes[i].key, "%u", (unsigned)timedRefresh(rx, ry, rw, rh));
    delay(500);
  }
  d.setEpdMode(epd_mode_t::epd_quality);
  d.setFont(&fonts::DejaVu24);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(top_left);
  d.startWrite();
  d.fillRect(0, 0, 420, 40, TFT_WHITE);
  d.drawString("16 grey bars; press to continue", 8, 8);
  d.endWrite();
  d.display(0, 0, 420, 40);
  d.waitDisplay();
  waitInput();
}

// SD wiring identical to src/config_sd.cpp. Duplicated (not reused) because that
// file reads config.yaml contents, which the probe must never do.
constexpr int kSdSck = 14, kSdMiso = 13, kSdMosi = 12, kSdCs = 4;
constexpr uint32_t kSdHz = 20000000;

void stepSd() {
  auto &d = M5.Display;
  prompt("SD card test", "no card? hold to skip");
  Input in = waitInput(2000);
  if (in.kind == Kind::Long) {
    kvs("sd", "skipped");
    return;
  }

  d.waitDisplay();  // never touch the shared SPI bus during an EPD transfer
  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);
  SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);

  const char *result = "ok";
  if (!SD.begin(kSdCs, SPI, kSdHz)) {
    result = "nocard";
  } else {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
      result = "error";
    } else {
      int n = 0;
      for (File e = root.openNextFile(); e && n < 40; e = root.openNextFile(), n++) {
        // Names only; the file is never read.
        kv("sd_entry", "%s%s", e.name(), e.isDirectory() ? "/" : "");
        e.close();
      }
      kv("sd_entries", "%d", n);
      root.close();
    }
    SD.end();
  }
  digitalWrite(kSdCs, HIGH);
  kvs("sd", "%s", result);

  // Display must still work after the shared bus was used.
  d.setEpdMode(epd_mode_t::epd_quality);
  d.setRotation(0);
  d.startWrite();
  drawBars(true);
  d.fillRect(230, 200, 500, 140, TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::DejaVu40);
  d.drawString("Display after SD", 480, 240);
  d.setFont(&fonts::DejaVu24);
  d.drawString("OK? push = yes, up/down = no", 480, 300);
  d.endWrite();
  d.display();
  d.waitDisplay();
  kv("display_after_sd", "ok");

  Input c = waitInput();
  if (c.kind == Kind::Long) {
    kvs("display_after_sd_confirm", "skipped");
  } else {
    const bool yes = (g_pushButton >= 0) ? (c.button == g_pushButton) : false;
    kvs("display_after_sd_confirm", "%s", yes ? "yes" : "no");
  }
}

void stepDone() {
  auto &d = M5.Display;
  d.setEpdMode(epd_mode_t::epd_quality);
  d.setRotation(0);
  d.startWrite();
  d.fillScreen(TFT_WHITE);
  d.setTextColor(TFT_BLACK, TFT_WHITE);
  d.setTextDatum(top_left);
  d.setFont(&fonts::DejaVu40);
  d.drawString("Probe done", 20, 10);
  d.setFont(&fonts::DejaVu24);
  int row = 0;
  for (const auto &it : g_summary) {
    const int col = row / 11;
    const int line = row % 11;
    if (col > 1) break;
    std::string s = it.key + "=" + it.value;
    if (s.size() > 36) s.resize(36);
    d.drawString(s.c_str(), 20 + col * 480, 70 + line * 40);
    row++;
  }
  d.endWrite();
  d.display();
  d.waitDisplay();
  Serial.println("PROBE: done");
  Serial.flush();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("PROBE: stage=pre_begin");
  Serial.flush();

  // serial_baudrate defaults to 0 in M5Unified: M5.begin() does not open Serial itself.
  auto cfg = M5.config();
  const uint32_t defaultBaud = cfg.serial_baudrate;
  M5.begin(cfg);
  Serial.println("PROBE: stage=post_begin");
  Serial.flush();
  kvs("serial_baudrate_default", "%u", (unsigned)defaultBaud);
  Serial.flush();

  stepFacts();
  stepGate();
  stepRocker();
  stepTouch();
  stepEpd();
  stepSd();
  stepDone();
}

void loop() {
  poll();  // keep logging button events after the run
  delay(10);
}
