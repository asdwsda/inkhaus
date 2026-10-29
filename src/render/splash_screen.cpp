#include "splash_screen.h"

namespace render {

namespace {

constexpr int kLogoSize = 360;

void drawLogo(M5Canvas &c, int x, int y) {
  const uint8_t *bmp = logoBitmap(kLogoSize);
  if (!bmp) return;
  uint8_t *buf = static_cast<uint8_t *>(c.getBuffer());
  const int W = c.width(), H = c.height();
  for (int row = 0; row < kLogoSize; ++row) {
    const int py = y + row;
    if (py < 0 || py >= H) continue;
    for (int col = 0; col < kLogoSize; ++col) {
      const int a = bmp[row * kLogoSize + col];
      const int px = x + col;
      if (!a || px < 0 || px >= W) continue;
      uint8_t &d = buf[py * W + px];
      d = uint8_t(int(d) - (int(d) * a + 127) / 255);  // towards black (INK = 0)
    }
  }
}

}  // namespace

void drawSplash(M5Canvas &c, const char *version) {
  c.fillScreen(PAPER);
  const bool portrait = c.height() > c.width();
  // The name steps down to F40 when it would not fit 440 px at F56.
  const Font nameFont = textWidth(c, Font::F56, kAppName) > 440 ? Font::F40 : Font::F56;
  if (portrait) {
    drawLogo(c, 90, 170 + 20);
    drawTextCy(c, nameFont, kAppName, 270, 690, Align::Center, INK);
    drawTextCy(c, Font::F24, "for Home Assistant", 270, 752, Align::Center, DIM);
    c.fillRect(235, 796, 70, 2, OFFGREY);
    drawTextCy(c, Font::F24, version, 270, 840, Align::Center, INK);
    drawTextCy(c, Font::F18, "M5Paper e-ink remote", 270, 920, Align::Center, DIM);
  } else {
    drawLogo(c, 70, 70 + 20);
    drawTextCy(c, nameFont, kAppName, 500, 205, Align::Left, INK);
    drawTextCy(c, Font::F24, "for Home Assistant", 500, 266, Align::Left, DIM);
    c.fillRect(500, 306, 70, 2, OFFGREY);
    drawTextCy(c, Font::F24, version, 500, 346, Align::Left, INK);
    drawTextCy(c, Font::F18, "M5Paper e-ink remote", 500, 500, Align::Left, DIM);
  }
}

}  // namespace render
