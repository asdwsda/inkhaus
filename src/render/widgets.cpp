#include "widgets.h"

#include <algorithm>
#include <cmath>

#include "widget_view.h"

namespace render {

using config::WidgetType;
using uistate::Look;

namespace {

struct Ctx {
  M5Canvas &c;
  ui::Rect r;  // widget rect (absolute)
  int innerSub = 0;  // rocker INNER focus: sub-region id with the 4 px border
  int W() const { return r.w; }
  int X(int dx) const { return r.x + dx; }
  int Y(int dy) const { return r.y + dy; }
};

enum class FrameStyle { Solid, Dotted };

void drawFrame(Ctx &x, uint32_t fill, bool dotted) {
  if (dotted) {
    x.c.fillRoundRect(x.r.x, x.r.y, x.r.w, x.r.h, 9, PAPER);
    drawDashedBox(x.c, float(x.r.x + 1), float(x.r.y + 1), float(x.r.w - 2), float(x.r.h - 2), 8.0f, 2, 2.0f, 4.0f,
                  OFFGREY);
  } else {
    drawBox(x.c, x.r, fill, INK);
  }
}

// Pending ring of a toggle: dashed 3 px, centre line inset 5, radius 5, in the contrast colour.
void drawPendingRing(Ctx &x, uint32_t color) {
  drawDashedBox(x.c, float(x.r.x + 5), float(x.r.y + 5), float(x.r.w - 10), float(x.r.h - 10), 5.0f, 3, 8.0f, 6.0f,
                color);
}

Icon actionIcon(uistate::ActionIcon a) {
  using A = uistate::ActionIcon;
  switch (a) {
    case A::Flame: return Icon::Flame;
    case A::Snow: return Icon::Snow;
    case A::Dry: return Icon::Dry;
    case A::Fan: return Icon::Fan;
    case A::Pause: return Icon::Pause;
    case A::Auto: return Icon::Auto;
    case A::Power: return Icon::Power;
    default: return Icon::Count;
  }
}

Icon weatherIcon(uistate::WeatherIcon w) {
  using W = uistate::WeatherIcon;
  switch (w) {
    case W::Sunny: return Icon::Sunny;
    case W::ClearNight: return Icon::Moon;
    case W::PartlyCloudy: return Icon::Partlycloudy;
    case W::Cloudy: return Icon::Cloud;
    case W::Rainy:
    case W::Pouring: return Icon::Rainy;
    case W::Snowy: return Icon::Snowy;
    case W::Fog: return Icon::Fog;
    case W::Windy: return Icon::Fan;
    case W::Lightning: return Icon::Lightning;
    default: return Icon::Unknown;
  }
}

// One button of a row: outline / filled / dashed pending / dotted unavailable, glyph centred.
void drawButton(Ctx &x, const ui::Rect &b, Icon glyph, bool filled, bool pending, bool unavailable,
                bool focus = false, bool hourglassGlyph = true) {
  M5Canvas &c = x.c;
  uint32_t glyphColor = INK;
  if (focus && !unavailable) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, INK);
    c.fillRoundRect(b.x + 4, b.y + 4, b.w - 8, b.h - 8, 5, PAPER);
    if (filled && !pending) {
      c.fillRoundRect(b.x + 6, b.y + 6, b.w - 12, b.h - 12, 3, INK);
      glyphColor = PAPER;
    }
    if (pending && hourglassGlyph) glyph = Icon::Hourglass;
  } else if (unavailable) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, PAPER);
    drawDashedBox(c, float(b.x + 1), float(b.y + 1), float(b.w - 2), float(b.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
    glyphColor = OFFGREY;
  } else if (pending) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, PAPER);
    drawDashedBox(c, b.x + 1.5f, b.y + 1.5f, float(b.w - 3), float(b.h - 3), 8.0f, 3, 8.0f, 6.0f, INK);
    if (hourglassGlyph) glyph = Icon::Hourglass;  // cover/vacuum; climate keeps its glyph (badge shows it)
  } else if (filled) {
    drawBox(c, b, INK, INK);
    glyphColor = PAPER;
  } else {
    drawBox(c, b, PAPER, INK);
  }
  drawIcon(c, glyph, 40, b.x + 12, b.y + 12, glyphColor);
}

void drawBadge(Ctx &x, int dx, int dy, Icon icon, uint32_t color) {
  drawIcon(x.c, icon, 24, x.X(dx), x.Y(dy), color);
}

uint32_t textColor(Look look, uint32_t normal) { return look == Look::Normal ? normal : OFFGREY; }

// ---- cover, wide ---------------------------------------------------------------------------------

void drawCover(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const auto v = uistate::coverView(d.members, d.group);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 252), x.X(12), x.Y(44), Align::Left,
           textColor(v.look == Look::Unavailable ? v.look : Look::Normal, INK));
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, v.state, W - 284), x.X(12), x.Y(77), Align::Left,
           INK);
  if (una) drawBadge(x, W - 264, 57, Icon::Warn, OFFGREY);
  else if (v.alertBadge || d.failed) drawBadge(x, W - 264, 57, Icon::Alert, INK);
  auto subs = ui::subRegions(WidgetType::Cover, config::WidgetSize{}, x.r);
  const Icon glyphs[3] = {Icon::Up, Icon::Stop, Icon::Down};
  const bool filled[3] = {v.upFilled, false, v.downFilled};
  for (int i = 0; i < 3; ++i)
    drawButton(x, subs[size_t(i)].rect, glyphs[i], filled[i], d.pending && d.pendingSub == i + 1, una,
               x.innerSub == i + 1);
}

// ---- toggles: light, switch, automation, binary sensor -------------------------------------------

void drawToggle(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const WidgetType type = d.type;
  const bool binary = type == WidgetType::BinarySensor;
  const bool pend = d.pending && !binary;
  const EntityState *st = binary ? (d.members.empty() ? nullptr : d.members[0]) : uistate::toggleSummary(d.members);
  const bool failed = d.failed && !binary;
  const bool badge = pend || failed || (st && !st->available);
  const auto v = uistate::toggleView(type, st, badge);
  const bool una = v.look == Look::Unavailable;
  const bool inkFill = v.on && !binary && !una;
  drawFrame(x, inkFill ? INK : PAPER, una);
  const int W = x.W();
  const uint32_t fg = una ? OFFGREY : (inkFill ? PAPER : INK);
  const uint32_t sub = inkFill ? PAPER : INK;  // dynamic text: pure black (or white on the ink fill)
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 24), x.X(12), x.Y(38), Align::Left, fg);
  Icon icon = Icon::Bulb;
  switch (type) {
    case WidgetType::Light: icon = v.on ? Icon::Bulb : Icon::BulbOff; break;
    case WidgetType::Switch: icon = Icon::Plug; break;
    case WidgetType::Automation: icon = Icon::Auto; break;
    case WidgetType::BinarySensor: icon = v.on ? Icon::WindowOn : Icon::WindowOff; break;
    default: break;
  }
  if (binary && una) drawIcon(x.c, Icon::Warn, 40, x.X(12), x.Y(49), OFFGREY);
  else drawIcon(x.c, icon, 40, x.X(12), x.Y(49), fg);
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, v.state, W - 70), x.X(58), x.Y(77), Align::Left, sub);
  if (una && !binary) drawBadge(x, W - 36, 57, Icon::Warn, OFFGREY);
  else if (failed) drawBadge(x, W - 36, 57, Icon::Alert, inkFill ? PAPER : INK);
  else if (pend) {
    drawBadge(x, W - 36, 57, Icon::Hourglass, inkFill ? PAPER : INK);
    drawPendingRing(x, inkFill ? PAPER : INK);
  }
}

// ---- climate, wide -------------------------------------------------------------------------------

void drawClimate(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::climateView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  const int Lx = W - 288;
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 328), x.X(12), x.Y(44), Align::Left, una ? OFFGREY : INK);
  const uint32_t dataColor = INK;
  drawText(x.c, Font::F24, v.temp, x.X(12), x.Y(77), Align::Left, dataColor);
  if (v.look == Look::Normal) {
    if (!v.hum.empty()) {
      const int humEnd = 120 + textWidth(x.c, Font::F24, v.hum);
      // The drop icon is dropped (text stays) if the run would pass Lx, so the action icon keeps its place.
      if (humEnd + 12 + 24 <= Lx) drawIcon(x.c, Icon::Drop, 24, x.X(92), x.Y(57), INK);
      drawText(x.c, Font::F24, v.hum, x.X(120), x.Y(77), Align::Left, INK);
    }
    const Icon ai = actionIcon(v.act);
    if (ai != Icon::Count) drawIcon(x.c, ai, 24, x.X(186), x.Y(57), INK);
  }
  if (una) drawBadge(x, W - 312, 24, Icon::Warn, OFFGREY);
  else if (d.failed) drawBadge(x, W - 312, 24, Icon::Alert, INK);
  else if (d.pending) drawBadge(x, W - 312, 24, Icon::Hourglass, INK);
  auto subs = ui::subRegions(WidgetType::Climate, config::WidgetSize{}, x.r);
  drawButton(x, subs[0].rect, Icon::Minus, false, d.pending && d.pendingSub == 1, una, x.innerSub == 1,
             false);
  drawButton(x, subs[1].rect, Icon::Plus, false, d.pending && d.pendingSub == 2, una, x.innerSub == 2,
             false);
  drawText(x.c, Font::F40, v.target, x.X(W - 144), x.Y(66), Align::Center, dataColor);
}

// ---- sensor --------------------------------------------------------------------------------------

void drawSensor(Ctx &x, const ui::Placement &pl, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::sensorView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  const uint32_t vc = INK;
  if (pl.cellW >= 2) {
    // 2x1: name left (up to two lines), value right.
    const int nameW = W - 232;
    auto lines = wrapText(x.c, Font::F24, title, nameW, 2);
    if (lines.size() >= 2) {
      drawText(x.c, Font::F24, lines[0], x.X(12), x.Y(44), Align::Left, INK);
      drawText(x.c, Font::F24, lines[1], x.X(12), x.Y(77), Align::Left, INK);
    } else if (!lines.empty()) {
      drawText(x.c, Font::F24, lines[0], x.X(12), x.Y(60), Align::Left, INK);
    }
    std::string unit = v.unit;
    if (v.textState)
      drawText(x.c, Font::F24, fitText(x.c, Font::F24, v.value, 220), x.X(W - 12), x.Y(60), Align::Right, vc);
    else
      drawRun(x.c, Font::F56, v.value, Font::F40, unit, x.X(W - 12), x.Y(71), Align::Right, vc);
    if (una) drawBadge(x, W - 224, 40, Icon::Warn, OFFGREY);
    return;
  }
  const int maxW = W - 24;
  if (!v.numeric) {
    drawText(x.c, Font::F24, fitText(x.c, Font::F24, v.value, maxW), x.X(W / 2), x.Y(47), Align::Center, vc);
  } else {
    std::string value = v.value;
    Font vf = Font::F40, uf = Font::F24;
    int baseline = 53;
    if (runWidth(x.c, vf, value, uf, v.unit) > maxW) {
      const size_t dot = value.find('.');
      if (dot != std::string::npos) value.erase(dot);  // drop the decimals
    }
    if (runWidth(x.c, vf, value, uf, v.unit) > maxW) {
      vf = Font::F24;
      uf = Font::F18;
      baseline = 47;
    }
    drawRun(x.c, vf, value, uf, v.unit, x.X(W / 2), x.Y(baseline), Align::Center, vc);
  }
  if (una) drawBadge(x, W - 36, 12, Icon::Warn, OFFGREY);
  drawText(x.c, Font::F18, fitText(x.c, Font::F18, title, maxW), x.X(W / 2), x.Y(84), Align::Center,
           INK);
}

// ---- weather -------------------------------------------------------------------------------------

void drawWeather(Ctx &x, const ui::Placement &pl, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::weatherView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  const uint32_t tc = INK;
  const Icon icon = una ? Icon::Unknown : weatherIcon(v.icon);
  drawIcon(x.c, icon, 48, x.X(12), x.Y(28), una ? OFFGREY : INK);
  if (pl.cellW >= 2) {
    drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 174), x.X(68), x.Y(44), Align::Left, INK);
    if (una) {
      drawBadge(x, 68, 57, Icon::Warn, OFFGREY);
    } else if (v.look == Look::Normal) {
      drawText(x.c, Font::F24, v.cond, x.X(68), x.Y(77), Align::Left, INK);
      if (!v.hum.empty()) {
        const int dropX = 68 + textWidth(x.c, Font::F24, v.cond) + 12;
        drawIcon(x.c, Icon::Drop, 24, x.X(dropX), x.Y(57), INK);
        drawText(x.c, Font::F24, v.hum, x.X(dropX + 28), x.Y(77), Align::Left, INK);
      }
    }
    drawRun(x.c, Font::F40, v.temp, Font::F24, v.look == Look::Normal ? v.unit : "", x.X(W - 13), x.Y(66),
            Align::Right, tc);
    return;
  }
  drawRun(x.c, Font::F40, v.temp, Font::F24, v.look == Look::Normal ? v.unit : "", x.X(64), x.Y(53), Align::Left, tc);
  if (una) drawBadge(x, 64, 66, Icon::Warn, OFFGREY);
  else if (v.look == Look::Normal)
    drawText(x.c, Font::F18, fitText(x.c, Font::F18, v.cond, W - 76), x.X(64), x.Y(84), Align::Left, INK);
}

// ---- vacuum, wide --------------------------------------------------------------------------------

void drawVacuum(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::vacuumView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 252), x.X(12), x.Y(44), Align::Left, una ? OFFGREY : INK);
  const uint32_t sc = INK;
  const int statusMax = v.error ? W - 284 : W - 284;
  const std::string status = fitText(x.c, Font::F24, v.status, statusMax);
  drawText(x.c, Font::F24, status, x.X(12), x.Y(77), Align::Left, sc);
  if (v.showBattery) {
    const int ix = 12 + textWidth(x.c, Font::F24, status) + 12;
    drawIcon(x.c, Icon::Batt, 24, x.X(ix), x.Y(57), INK);
    drawText(x.c, Font::F18, std::to_string(v.battery) + "%", x.X(ix + 28), x.Y(75), Align::Left, INK);
  }
  if (una) drawBadge(x, W - 264, 57, Icon::Warn, OFFGREY);
  else if (v.error || d.failed) drawBadge(x, W - 264, 57, Icon::Alert, INK);
  auto subs = ui::subRegions(WidgetType::Vacuum, config::WidgetSize{}, x.r);
  const Icon glyphs[3] = {Icon::Play, Icon::Pause, Icon::Dock};
  const bool filled[3] = {v.startFilled, v.pauseFilled, v.dockFilled};
  for (int i = 0; i < 3; ++i)
    drawButton(x, subs[size_t(i)].rect, glyphs[i], filled[i], d.pending && d.pendingSub == i + 1, una,
               x.innerSub == i + 1);
}

}  // namespace

void drawWidget(M5Canvas &c, const ui::Placement &pl, const uistate::WidgetData &data, const std::string &title,
                int innerSub) {
  Ctx x{c, pl.rect, innerSub};
  switch (data.type) {
    case WidgetType::Cover: drawCover(x, data, title); break;
    case WidgetType::Light:
    case WidgetType::Switch:
    case WidgetType::Automation:
    case WidgetType::BinarySensor: drawToggle(x, data, title); break;
    case WidgetType::Climate: drawClimate(x, data, title); break;
    case WidgetType::Sensor: drawSensor(x, pl, data, title); break;
    case WidgetType::Weather: drawWeather(x, pl, data, title); break;
    case WidgetType::Vacuum: drawVacuum(x, data, title); break;
  }
}

}  // namespace render
