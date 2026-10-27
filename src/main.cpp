#include <M5Unified.h>

#include "version.h"

static const char *const kTitle = "M5Paper HA remote";

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.printf("%s v%s\n", kTitle, FW_VERSION);

  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.setRotation(0);
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);

  const int cx = M5.Display.width() / 2;
  const int cy = M5.Display.height() / 2;
  M5.Display.setTextSize(4);
  M5.Display.drawString(kTitle, cx, cy - 30);
  M5.Display.setTextSize(3);
  M5.Display.drawString("v" FW_VERSION, cx, cy + 30);
  M5.Display.endWrite();
  M5.Display.display();  // single full refresh
  M5.Display.waitDisplay();
}

void loop() {
  delay(1000);
}
