// E-ink persistence and refresh-mode experiment (task 015).
//
// Serial log lines are "EPD: <key>=<value>". The user answers with the rocker:
// UP = yes, DOWN = no, PUSH = skip (a push followed by UP/DOWN within 150 ms counts as that direction).
// The step lives in NVS (namespace "epdtest"), so the test survives its own ESP.restart().
//
// Steps (each starts with a fresh boot, the start-up kind is chosen from the step):
//   A_DRAW   stock start-up (M5.begin, IT8951 hardware reset): reference screen, restart
//   A_CHECK  stock start-up: only a bottom strip is refreshed, ask if PERSIST is still there
//   B_DRAW   start-up WITHOUT the IT8951 reset (own LGFX_Device, no M5Unified): reference, restart
//   B_CHECK  start-up without reset: strip question, then touch + partial refresh check
//   MODE     stock start-up: four boxes, refresh modes A-D
//   AA       anti-aliased vs plain text in epd_fastest
//   DONE     summary

#include <M5Unified.h>
#include <Preferences.h>
#include <esp_system.h>

#include "lgfx/v1/panel/Panel_IT8951.hpp"
#include "lgfx/v1/touch/Touch_GT911.hpp"

namespace {

// ---- steps and NVS -------------------------------------------------------------------------

enum Step : uint8_t {
  S_A_DRAW = 0,
  S_A_CHECK = 1,
  S_B_DRAW = 2,
  S_B_CHECK = 3,
  S_MODE = 4,
  S_AA = 5,
  S_DONE = 6,
};

const char *const kNs = "epdtest";
const char *const kKeyStep = "step";
const char *const kKeyRes = "res";  // "key=value\n" lines, for the final summary

uint8_t loadStep() {
  Preferences p;
  if (!p.begin(kNs, true)) return S_A_DRAW;
  const uint8_t s = p.getUChar(kKeyStep, S_A_DRAW);
  p.end();
  return s > S_DONE ? S_A_DRAW : s;
}

void saveStep(uint8_t s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar(kKeyStep, s);
  p.end();
}

void resetAll() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.clear();
  p.end();
}

String loadResults() {
  Preferences p;
  if (!p.begin(kNs, true)) return String();
  String r = p.getString(kKeyRes, "");
  p.end();
  return r;
}

// Logs the result on serial and keeps it in NVS for the summary.
void rec(const char *key, const char *value) {
  Serial.printf("EPD: %s=%s\n", key, value);
  Preferences p;
  if (!p.begin(kNs, false)) return;
  String r = p.getString(kKeyRes, "");
  if (r.length() < 1500) {
    r += key;
    r += '=';
    r += value;
    r += '\n';
    p.putString(kKeyRes, r);
  }
  p.end();
}

void log(const char *key, const char *value) { Serial.printf("EPD: %s=%s\n", key, value); }
void logInt(const char *key, long value) { Serial.printf("EPD: %s=%ld\n", key, value); }

[[noreturn]] void restartInto(uint8_t step) {
  saveStep(step);
  Serial.printf("EPD: restart_to_step=%u\n", (unsigned)step);
  Serial.flush();
  delay(100);
  ESP.restart();
  while (true) delay(1000);
}

// ---- display: stock (M5.Display) or without the IT8951 reset -------------------------------

// Same configuration as M5GFX's M5Paper autodetect (M5GFX.cpp:1695-1738), but built by hand so
// that Panel_IT8951::init(false) runs without toggling GPIO23 (Panel_Device.inl:72,84).
class NoResetDisplay : public lgfx::LGFX_Device {
  lgfx::Panel_IT8951 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Touch_GT911 _touch;

 public:
  NoResetDisplay() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI3_HOST;
      cfg.dma_channel = 1;
      cfg.spi_mode = 0;
      cfg.use_lock = true;
      cfg.pin_mosi = GPIO_NUM_12;
      cfg.pin_miso = GPIO_NUM_13;
      cfg.pin_sclk = GPIO_NUM_14;
      cfg.pin_dc = -1;
      cfg.spi_3wire = false;
      cfg.freq_write = 40000000;
      cfg.freq_read = 20000000;
      _bus.config(cfg);
      _panel.bus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.panel_height = 540;
      cfg.panel_width = 960;
      cfg.pin_cs = GPIO_NUM_15;
      cfg.pin_rst = GPIO_NUM_23;
      cfg.pin_busy = GPIO_NUM_27;
      cfg.offset_rotation = 3;
      _panel.config(cfg);
    }
    {
      auto cfg = _touch.config();
      cfg.pin_int = GPIO_NUM_36;
      cfg.pin_sda = GPIO_NUM_21;
      cfg.pin_scl = GPIO_NUM_22;
      cfg.i2c_port = I2C_NUM_1;
      cfg.freq = 400000;
      cfg.x_min = 0;
      cfg.x_max = 539;
      cfg.y_min = 0;
      cfg.y_max = 959;
      cfg.offset_rotation = 1;
      cfg.bus_shared = false;
      _touch.config(cfg);
      _panel.touch(&_touch);
    }
    setPanel(&_panel);
  }
};

NoResetDisplay g_noReset;
lgfx::LGFX_Device *g = nullptr;  // the display in use
bool g_noResetStart = false;

// Pins as set by M5GFX autodetect before it talks to the panel (M5GFX.cpp:1642-1656):
// GPIO2 = main power hold, GPIO15 = EPD CS, GPIO4 = SD CS (deselect, shared SPI bus).
void preparePinsNoReset() {
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(23, OUTPUT);  // IT8951 RST: held high, never pulsed
  digitalWrite(23, HIGH);
}

// ---- rocker (raw GPIO, identical for both start-ups) ----------------------------------------

constexpr int kPinUp = 37;    // BtnA
constexpr int kPinPush = 38;  // BtnB
constexpr int kPinDown = 39;  // BtnC

enum Answer { ANS_YES, ANS_NO, ANS_SKIP };

bool down(int pin) { return digitalRead(pin) == LOW; }
bool anyDown() { return down(kPinUp) || down(kPinPush) || down(kPinDown); }

void initButtons() {
  pinMode(kPinUp, INPUT);
  pinMode(kPinPush, INPUT);
  pinMode(kPinDown, INPUT);
}

void waitReleased() {
  while (anyDown()) delay(10);
  delay(50);
}

// A push often tilts the rocker a little: a push that is followed by UP/DOWN within kPushGraceMs
// is taken as that direction, not as a skip.
constexpr uint32_t kPushGraceMs = 150;

Answer waitAnswer() {
  waitReleased();
  while (true) {
    if (down(kPinUp)) {
      waitReleased();
      return ANS_YES;
    }
    if (down(kPinDown)) {
      waitReleased();
      return ANS_NO;
    }
    if (down(kPinPush)) {
      const uint32_t t0 = millis();
      while (millis() - t0 < kPushGraceMs) {
        if (down(kPinUp)) {
          waitReleased();
          return ANS_YES;
        }
        if (down(kPinDown)) {
          waitReleased();
          return ANS_NO;
        }
        delay(5);
      }
      waitReleased();
      return ANS_SKIP;
    }
    delay(10);
  }
}

// True when PUSH is held for holdMs without UP/DOWN being touched.
bool pushHeld(uint32_t holdMs) {
  if (!down(kPinPush)) return false;
  const uint32_t t0 = millis();
  while (down(kPinPush)) {
    if (down(kPinUp) || down(kPinDown)) return false;
    if (millis() - t0 >= holdMs) return true;
    delay(10);
  }
  return false;
}

const char *ansName(Answer a) { return a == ANS_YES ? "yes" : (a == ANS_NO ? "no" : "skipped"); }

// ---- drawing helpers -------------------------------------------------------------------------

constexpr int kStripY = 440;
constexpr int kStripH = 100;

void showStrip(const char *question, const char *hint = "UP = yes  -  DOWN = no  -  PUSH = skip") {
  g->setEpdMode(epd_mode_t::epd_fast);
  g->startWrite();
  g->fillRect(0, kStripY, g->width(), kStripH, TFT_WHITE);
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setTextDatum(middle_center);
  g->setTextSize(1);
  g->setFont(&fonts::DejaVu40);
  g->drawString(question, g->width() / 2, kStripY + 34);
  g->setFont(&fonts::DejaVu24);
  g->drawString(hint, g->width() / 2, kStripY + 80);
  g->endWrite();
  g->display(0, kStripY, g->width(), kStripH);
  g->waitDisplay();
}

void drawReference() {
  g->setEpdMode(epd_mode_t::epd_quality);
  g->startWrite();
  g->fillScreen(TFT_WHITE);
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setTextDatum(middle_center);
  const int w = g->width();
  g->setFont(&fonts::DejaVu40);
  g->setTextSize(3);
  g->drawString("PERSIST", w / 2, 150);
  g->setTextSize(1);
  g->setFont(&fonts::DejaVu72);
  g->drawString("1", w / 2, 320);
  constexpr int s = 60;
  g->fillRect(10, 10, s, s, TFT_BLACK);
  g->fillRect(w - 10 - s, 10, s, s, TFT_BLACK);
  g->fillRect(10, kStripY - 10 - s, s, s, TFT_BLACK);
  g->fillRect(w - 10 - s, kStripY - 10 - s, s, s, TFT_BLACK);
  g->endWrite();
  g->display();
  g->waitDisplay();
}

void clearScreenQuality() {
  g->setEpdMode(epd_mode_t::epd_quality);
  g->fillScreen(TFT_WHITE);
  g->display();
  g->waitDisplay();
}

// ---- steps -------------------------------------------------------------------------------------

void stepPersistDraw(const char *refKey, uint8_t nextStep) {
  drawReference();
  showStrip("PERSIST + 4 squares?");
  rec(refKey, ansName(waitAnswer()));
  g->waitDisplay();
  restartInto(nextStep);
}

void stepPersistCheck(const char *key, uint8_t nextStep, bool touchCheck) {
  // Deliberately nothing but the strip is drawn here.
  showStrip("PERSIST still visible?");
  rec(key, ansName(waitAnswer()));
  if (touchCheck) {
    // Touch and partial refresh after a start-up without reset.
    const int bw = 360, bh = 160;
    const int bx = (g->width() - bw) / 2, by = 120;
    g->setEpdMode(epd_mode_t::epd_fast);
    g->startWrite();
    g->fillRect(0, 0, g->width(), kStripY, TFT_WHITE);
    g->drawRect(bx, by, bw, bh, TFT_BLACK);
    g->drawRect(bx + 1, by + 1, bw - 2, bh - 2, TFT_BLACK);
    g->setTextColor(TFT_BLACK, TFT_WHITE);
    g->setTextDatum(middle_center);
    g->setFont(&fonts::DejaVu40);
    g->drawString("TAP", bx + bw / 2, by + bh / 2);
    g->endWrite();
    g->display(0, 0, g->width(), kStripY);
    g->waitDisplay();
    showStrip("Tap the box", "waiting 20 s");

    bool tapped = false;
    const uint32_t t0 = millis();
    while (millis() - t0 < 20000) {
      int32_t x, y;
      if (g->getTouch(&x, &y)) {
        logInt("touch_x", x);
        logInt("touch_y", y);
        if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
          tapped = true;
          break;
        }
      }
      delay(20);
    }
    rec("noreset_touch", tapped ? "ok" : "timeout");

    if (tapped) {
      g->setEpdMode(epd_mode_t::epd_fastest);
      g->startWrite();
      g->fillRect(bx, by, bw, bh, TFT_BLACK);
      g->endWrite();
      const uint32_t d0 = millis();
      g->display(bx, by, bw, bh);
      g->waitDisplay();
      logInt("noreset_partial_ms", (long)(millis() - d0));
      showStrip("Box turned black?");
      rec("noreset_partial", ansName(waitAnswer()));
    } else {
      rec("noreset_partial", "unsupported");
    }
  }
  restartInto(nextStep);
}

constexpr int kBoxes = 4;
constexpr int kBoxSize = 200;
constexpr int kBoxGap = 40;
constexpr int kBoxY = 110;
int boxX(int i) { return 20 + i * (kBoxSize + kBoxGap); }

void drawDigit(int i, int n) {
  const int x = boxX(i);
  g->fillRect(x + 2, kBoxY + 2, kBoxSize - 4, kBoxSize - 4, TFT_WHITE);
  char s[2] = {(char)('0' + n), 0};
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setTextDatum(middle_center);
  g->setTextSize(1);
  g->setFont(&fonts::DejaVu72);
  g->drawString(s, x + kBoxSize / 2, kBoxY + kBoxSize / 2);
}

// One update of box i with digit n. Returns the time from first refresh command to done, in ms.
uint32_t updateBox(int i, int n) {
  const int x = boxX(i);
  const uint32_t t0 = millis();
  switch (i) {
    case 0:  // A: epd_fastest, draw over
    case 1:  // B: epd_fast
    case 2:  // C: epd_text
      g->setEpdMode(i == 0 ? epd_mode_t::epd_fastest
                           : (i == 1 ? epd_mode_t::epd_fast : epd_mode_t::epd_text));
      g->startWrite();
      drawDigit(i, n);
      g->endWrite();
      g->display(x, kBoxY, kBoxSize, kBoxSize);
      g->waitDisplay();
      break;
    default:  // D: white fill refreshed with epd_quality, then the digit in epd_fastest
      g->setEpdMode(epd_mode_t::epd_quality);
      g->startWrite();
      g->fillRect(x + 2, kBoxY + 2, kBoxSize - 4, kBoxSize - 4, TFT_WHITE);
      g->endWrite();
      g->display(x, kBoxY, kBoxSize, kBoxSize);
      g->waitDisplay();
      g->setEpdMode(epd_mode_t::epd_fastest);
      g->startWrite();
      drawDigit(i, n);
      g->endWrite();
      g->display(x, kBoxY, kBoxSize, kBoxSize);
      g->waitDisplay();
      break;
  }
  return millis() - t0;
}

void stepMode() {
  g->setEpdMode(epd_mode_t::epd_quality);
  g->startWrite();
  g->fillScreen(TFT_WHITE);
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setTextDatum(middle_center);
  g->setTextSize(1);
  const char *labels[kBoxes] = {"A", "B", "C", "D"};
  for (int i = 0; i < kBoxes; ++i) {
    g->setFont(&fonts::DejaVu40);
    g->drawString(labels[i], boxX(i) + kBoxSize / 2, kBoxY - 35);
    g->drawRect(boxX(i), kBoxY, kBoxSize, kBoxSize, TFT_BLACK);
    g->drawRect(boxX(i) + 1, kBoxY + 1, kBoxSize - 2, kBoxSize - 2, TFT_BLACK);
  }
  g->endWrite();
  g->display();
  g->waitDisplay();
  showStrip("Watch boxes A-D", "counting 1 to 6");
  delay(1500);

  uint32_t total[kBoxes] = {0, 0, 0, 0};
  for (int n = 1; n <= 6; ++n) {
    for (int i = 0; i < kBoxes; ++i) {
      const uint32_t ms = updateBox(i, n);
      total[i] += ms;
      Serial.printf("EPD: mode_%c_n%d_ms=%u\n", 'A' + i, n, (unsigned)ms);
    }
    delay(1000);
  }

  for (int i = 0; i < kBoxes; ++i) {
    char q[32], key[24], val[16];
    snprintf(q, sizeof(q), "Box %c clean?", 'A' + i);
    showStrip(q, "UP = yes (clean)  -  DOWN = no (ghosts)  -  PUSH = skip");
    snprintf(key, sizeof(key), "ghost_%c", 'A' + i);
    rec(key, ansName(waitAnswer()));
    snprintf(key, sizeof(key), "mode_%c_ms", 'A' + i);
    snprintf(val, sizeof(val), "%u", (unsigned)(total[i] / 6));
    rec(key, val);
  }
  restartInto(S_AA);
}

void stepAa() {
  clearScreenQuality();
  const char *word = "Kitchen 24";
  constexpr int kTextX = 150;
  constexpr int kRowAY = 130;
  constexpr int kRowBY = 280;
  constexpr int kSprW = 560;
  constexpr int kSprH = 100;

  // Plain text: render anti-aliased into an 8-bit gray sprite, then threshold to black.
  lgfx::LGFX_Sprite spr(g);
  spr.setColorDepth(lgfx::grayscale_8bit);
  spr.setPsram(true);
  const bool sprOk = spr.createSprite(kSprW, kSprH) != nullptr;
  if (sprOk) {
    spr.fillSprite(TFT_WHITE);
    spr.setTextColor(TFT_BLACK, TFT_WHITE);
    spr.setTextDatum(middle_left);
    spr.setFont(&fonts::DejaVu40);
    spr.setTextSize(2);
    spr.drawString(word, 0, kSprH / 2);
  } else {
    log("aa_sprite", "alloc_failed");
  }

  g->setEpdMode(epd_mode_t::epd_fastest);
  g->startWrite();
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setFont(&fonts::DejaVu40);
  g->setTextDatum(middle_center);
  g->setTextSize(1);
  g->drawString("A", 70, kRowAY);
  g->drawString("B", 70, kRowBY);
  g->setTextDatum(middle_left);
  g->setTextSize(2);
  g->drawString(word, kTextX, kRowAY);
  if (sprOk) {
    for (int y = 0; y < kSprH; ++y) {
      for (int x = 0; x < kSprW; ++x) {
        if (spr.readPixelValue(x, y) < 128) {
          g->drawPixel(kTextX + x, kRowBY - kSprH / 2 + y, TFT_BLACK);
        }
      }
    }
  }
  g->setTextSize(1);
  g->endWrite();
  g->display(0, 40, g->width(), 340);
  g->waitDisplay();
  spr.deleteSprite();

  showStrip("A smooth, B plain. Cleaner?", "UP = A cleaner  -  DOWN = B cleaner  -  PUSH = skip");
  waitReleased();
  const char *res = "same";
  while (true) {
    if (down(kPinUp)) { res = "aa"; break; }
    if (down(kPinDown)) { res = "noaa"; break; }
    if (down(kPinPush)) {
      const uint32_t t0 = millis();
      while (millis() - t0 < kPushGraceMs && !down(kPinUp) && !down(kPinDown)) delay(5);
      res = down(kPinUp) ? "aa" : (down(kPinDown) ? "noaa" : "skipped");
      break;
    }
    delay(10);
  }
  waitReleased();
  rec("aa_fastest", res);
  restartInto(S_DONE);
}

void stepDone() {
  Serial.println("EPD: done");
  const String r = loadResults();
  Serial.printf("EPD: summary_begin\n%sEPD: summary_end\n", r.c_str());

  g->setEpdMode(epd_mode_t::epd_quality);
  g->startWrite();
  g->fillScreen(TFT_WHITE);
  g->setTextColor(TFT_BLACK, TFT_WHITE);
  g->setTextDatum(top_left);
  g->setTextSize(1);
  g->setFont(&fonts::DejaVu24);
  g->drawString("EPD test done", 20, 8);
  g->setFont(&fonts::DejaVu18);
  int y = 48;
  int start = 0;
  int col = 0;
  while (start < (int)r.length() && y < 520) {
    int nl = r.indexOf('\n', start);
    if (nl < 0) nl = r.length();
    const String line = r.substring(start, nl);
    g->drawString(line, 20 + col * 480, y);
    y += 24;
    if (y > 500 && col == 0) {
      col = 1;
      y = 48;
    }
    start = nl + 1;
  }
  g->setFont(&fonts::DejaVu18);
  g->drawString("hold PUSH 2 s = run again", 520, 8);
  g->endWrite();
  g->display();
  g->waitDisplay();

  while (true) {
    if (pushHeld(2000)) {
      resetAll();
      restartInto(S_A_DRAW);
    }
    delay(20);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = micros();
  const uint8_t step = loadStep();
  g_noResetStart = (step == S_B_DRAW || step == S_B_CHECK);

  log("epdtest", "start");
  logInt("step", step);
  logInt("reset_reason", (long)esp_reset_reason());
  log("start", g_noResetStart ? "noreset" : "stock");
  initButtons();

  if (g_noResetStart) {
    // Variant B: no M5.begin() (it would run the autodetect with its hardcoded IT8951 reset,
    // M5GFX.cpp:1639). Holding PUSH at power-up gives up on this variant.
    preparePinsNoReset();
    if (down(kPinPush)) {
      rec("persist_noreset", "unsupported");
      restartInto(S_MODE);
    }
    g_noReset.init_without_reset(false);
    g = &g_noReset;
  } else {
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
    cfg.external_display_value = 0;
    M5.begin(cfg);
    g = &M5.Display;
  }
  g->setRotation(1);  // landscape 960x540
  logInt("start_ms", (long)((micros() - t0) / 1000));
  logInt("width", g->width());
  logInt("height", g->height());

  switch (step) {
    case S_A_DRAW: stepPersistDraw("persist_ref_stock", S_A_CHECK); break;
    case S_A_CHECK: stepPersistCheck("persist_stock", S_B_DRAW, false); break;
    case S_B_DRAW: stepPersistDraw("persist_ref_noreset", S_B_CHECK); break;
    case S_B_CHECK: stepPersistCheck("persist_noreset", S_MODE, true); break;
    case S_MODE: stepMode(); break;
    case S_AA: stepAa(); break;
    default: stepDone(); break;
  }
}

void loop() { delay(1000); }
