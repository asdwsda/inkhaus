#include "sleep_screen_draw.h"

#include <algorithm>

namespace render {

namespace {

using sleepscreen::BlockId;
using sleepscreen::Cond;
using sleepscreen::EventKind;
using sleepscreen::Placement;
using sleepscreen::View;

Icon conditionIcon(Cond c) {
  switch (c) {
    case Cond::Sunny: return Icon::Sunny;
    case Cond::ClearNight: return Icon::Moon;
    case Cond::PartlyCloudy: return Icon::Partlycloudy;
    case Cond::Cloudy: return Icon::Cloud;
    case Cond::Rainy:
    case Cond::Pouring: return Icon::Rainy;
    case Cond::Snowy: return Icon::Snowy;
    case Cond::Fog: return Icon::Fog;
    case Cond::Windy: return Icon::Fan;
    case Cond::Lightning: return Icon::Lightning;
    default: return Icon::Unknown;
  }
}

void caption(M5Canvas &c, const char *text, int x, int y) { drawTextCy(c, Font::F18, text, x, y + 11, Align::Left, DIM); }

// Value F40 + unit F24 sharing one baseline, the value centred on `cy`.
void valueRun(M5Canvas &c, const std::string &value, const std::string &unit, int x, int cy) {
  drawRun(c, Font::F40, value, Font::F24, unit, x, cy + baselineK(Font::F40), Align::Left, INK);
}

void drawDate(M5Canvas &c, const View &v, const Placement &p) {
  drawTextCy(c, Font::F56, v.date.weekday, p.rect.x, p.rect.y + 38, Align::Left, INK);
  drawTextCy(c, Font::F56, v.date.date, p.rect.x, p.rect.y + 110, Align::Left, INK);
}

void drawUpdated(M5Canvas &c, const View &v, const Placement &p) {
  const int x = p.rect.x, y = p.rect.y;
  Font f = v.updated.small ? Font::F24 : Font::F40;
  if (f == Font::F40 && textWidth(c, f, v.updated.line1) > p.rect.w) f = Font::F24;  // fit rule: F40, then F24
  drawTextCy(c, f, fitText(c, f, v.updated.line1, p.rect.w), x, y + 24, Align::Left, INK);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, v.updated.line2, p.rect.w), x, y + 70, Align::Left, INK);
}

// Battery at 2x STYLE 5.4: body 72x40 INK, inner 64x32 PAPER, level fill, nub (72, 10, 8, 20).
void drawBigBattery(M5Canvas &c, int x, int cy, int pct, bool low) {
  const int y = cy - 20;
  c.fillRect(x, y, 72, 40, INK);
  c.fillRect(x + 4, y + 4, 64, 32, PAPER);
  if (low) {
    c.fillRect(x + 32, y + 8, 8, 14, INK);  // "!"
    c.fillRect(x + 32, y + 26, 8, 6, INK);
  } else {
    c.fillRect(x + 4, y + 4, (64 * pct + 50) / 100, 32, INK);
  }
  c.fillRect(x + 72, y + 10, 8, 20, INK);
}

void drawStatus(M5Canvas &c, const View &v, const Placement &p) {
  const int x = p.rect.x, y = p.rect.y;
  const int x2 = x + v.layout.climateColumnDx;
  caption(c, "Battery", x, y);
  caption(c, "Indoor", x2, y);
  drawBigBattery(c, x, y + 52, v.battery.pct, v.battery.low);
  drawTextCy(c, Font::F40, v.battery.percent, x + 92, y + 52, Align::Left, INK);
  drawTextCy(c, Font::F24, v.battery.sub, x, y + 104, Align::Left, INK);
  if (v.climate.have) valueRun(c, v.climate.temp, v.climate.tempUnit, x2, y + 52);
  else drawTextCy(c, Font::F40, "-", x2, y + 52, Align::Left, INK);
  drawIcon(c, Icon::Drop, 24, x2, y + 104 - 12, INK);
  if (v.climate.have) valueRun(c, v.climate.hum, v.climate.humUnit, x2 + 32, y + 104);
  else drawTextCy(c, Font::F40, "-", x2 + 32, y + 104, Align::Left, INK);
}

void drawWeather(M5Canvas &c, const View &v, const Placement &p) {
  const int x = p.rect.x, y = p.rect.y;
  caption(c, "Weather", x, y);
  const auto &w = v.weather;
  drawIcon(c, w.unavailable ? Icon::Warn : conditionIcon(w.cond), 48, x, y + 54 - 24, w.unavailable ? OFFGREY : INK);
  valueRun(c, w.temp, w.unit, x + 64, y + 54);
  const int tx = std::max(x + 160, x + 64 + runWidth(c, Font::F40, w.temp, Font::F24, w.unit) + 16);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, w.condition, x + p.rect.w - tx), tx, y + 54, Align::Left, INK);
}

void drawEvent(M5Canvas &c, const View &v, const Placement &p) {
  const int x = p.rect.x, y = p.rect.y, W = p.rect.w;
  caption(c, "Next event", x, y);
  const auto &e = v.event;
  const int tx = x + 132;
  switch (e.kind) {
    case EventKind::Unavailable:
      drawTextCy(c, Font::F24, e.time, tx, y + 54, Align::Left, INK);
      return;
    case EventKind::None:
      drawTextCy(c, Font::F24, e.time, x, y + 54, Align::Left, INK);
      return;
    case EventKind::AllDay: drawTextCy(c, Font::F24, e.time, x, y + 54, Align::Left, INK); break;
    case EventKind::Timed: drawTextCy(c, Font::F40, e.time, x, y + 54, Align::Left, INK); break;
  }
  drawTextCy(c, Font::F24, fitText(c, Font::F24, e.title, x + W - tx), tx, y + 54, Align::Left, INK);
  if (!e.day.empty()) drawTextCy(c, Font::F18, e.day, x, y + 94, Align::Left, INK);
}

void drawNotifications(M5Canvas &c, const View &v, const Placement &p) {
  const int x = p.rect.x, y = p.rect.y;
  caption(c, "Notifications", x, y);
  drawTextCy(c, Font::F40, v.notifications.count, x, y + 54, Align::Left, INK);
  const int tx = x + std::max(44, textWidth(c, Font::F40, v.notifications.count) + 12);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, v.notifications.title, x + p.rect.w - tx), tx, y + 54, Align::Left, INK);
}

void drawFooter(M5Canvas &c, const View &v) {
  const auto &L = v.layout;
  c.fillRect(L.footerRule.x, L.footerRule.y, L.footerRule.w, L.footerRule.h, OFFGREY);
  if (!v.landscape) {
    drawIcon(c, Icon::Moon, 40, 16, 900, INK);
    drawTextCy(c, Font::F24, v.footerLabel, 68, 906, Align::Left, INK);
    drawTextCy(c, Font::F18, fitText(c, Font::F18, v.footerHint, 524 - 68), 68, 936, Align::Left, INK);
  } else {
    drawIcon(c, Icon::Moon, 24, 16, 500, INK);
    drawTextCy(c, Font::F24, v.footerLabel, 52, 512, Align::Left, INK);
    const int hintLeft = 52 + textWidth(c, Font::F24, v.footerLabel) + 16;
    drawTextCy(c, Font::F18, fitText(c, Font::F18, v.footerHint, 944 - hintLeft), 944, 512, Align::Right, INK);
  }
}

}  // namespace

void drawSleepScreen(M5Canvas &c, const View &v) {
  c.fillScreen(PAPER);
  const auto &L = v.layout;
  for (size_t i = 0; i < L.blocks.size(); ++i) {
    const Placement &p = L.blocks[i];
    switch (p.id) {
      case BlockId::kDate: drawDate(c, v, p); break;
      case BlockId::kUpdated: drawUpdated(c, v, p); break;
      case BlockId::kBattery: drawStatus(c, v, p); break;
      case BlockId::kWeather: drawWeather(c, v, p); break;
      case BlockId::kEvent: drawEvent(c, v, p); break;
      case BlockId::kNotifications: drawNotifications(c, v, p); break;
      default: break;
    }
    // Hairline in the middle of the 24 px gap to the next block of the same column.
    if (i + 1 < L.blocks.size() && L.blocks[i + 1].rect.x == p.rect.x)
      c.fillRect(p.rect.x, p.rect.y + p.rect.h + 12, p.rect.w, 1, OFFGREY);
  }
  if (L.divider.w > 0) c.fillRect(L.divider.x, L.divider.y, L.divider.w, L.divider.h, OFFGREY);
  drawFooter(c, v);
}

}  // namespace render
