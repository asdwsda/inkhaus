#include "widgets.h"

#include <algorithm>
#include <cmath>

#include "text_fit.h"
#include "weather_forecast_draw.h"
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

Icon stateIcon(uistate::StateIcon i) {
  using S = uistate::StateIcon;
  switch (i) {
    case S::WindowOn: return Icon::WindowOn;
    case S::WindowOff: return Icon::WindowOff;
    case S::DoorOn: return Icon::DoorOn;
    case S::DoorOff: return Icon::DoorOff;
    case S::GarageOn: return Icon::GarageOn;
    case S::GarageOff: return Icon::GarageOff;
    case S::MotionOn: return Icon::MotionOn;
    case S::MotionOff: return Icon::MotionOff;
    case S::PersonOn: return Icon::PersonOn;
    case S::PersonOff: return Icon::PersonOff;
    case S::PuddleOn: return Icon::PuddleOn;
    case S::PuddleOff: return Icon::PuddleOff;
    case S::SmokeOn: return Icon::SmokeOn;
    case S::SmokeOff: return Icon::SmokeOff;
    case S::GasOn: return Icon::GasOn;
    case S::GasOff: return Icon::GasOff;
    case S::ProblemOn: return Icon::ProblemOn;
    case S::ProblemOff: return Icon::ProblemOff;
    case S::BatteryLow: return Icon::BatteryLow;
    case S::Battery: return Icon::Batt;
    case S::Wifi: return Icon::Wifi;
    case S::WifiOff: return Icon::WifiOff;
    case S::LockOpen: return Icon::LockOpen;
    case S::LockClosed: return Icon::LockClosed;
    case S::Plug: return Icon::Plug;
    case S::PlugOff: return Icon::PlugOff;
    case S::Bolt: return Icon::Bolt;
    case S::BoltOff: return Icon::BoltOff;
    case S::DotOn: return Icon::DotOn;
    case S::DotOff: return Icon::DotOff;
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

// Narrow cells (task 094): F24 -> F18 -> "..." per layout.md 2b. Measured with the real fonts.
ui::TextMeasure measureOn(M5Canvas &c) {
  return [&c](bool small, const std::string &t) { return textWidth(c, small ? Font::F18 : Font::F24, t); };
}
constexpr const char *kEll = "\xE2\x80\xA6";  // same glyph as draw.cpp
constexpr int kNarrowW = 200;   // below this a widget uses the 1x1 text fallback (161 px cells, 118 px cells)
constexpr int kTinyW = 130;     // below this: icon above the text (portrait 4 columns)
constexpr int kCompactW = 400;  // wide types below this use the compact 2x1 layouts (portrait 3 columns)

uint32_t textColor(Look look, uint32_t normal) { return look == Look::Normal ? normal : OFFGREY; }

// ---- cover, wide ---------------------------------------------------------------------------------

// Compact state text: a trailing "NN%" replaces the whole text ("Opening 40%" -> "40%"), else the short form.
std::string compactState(const std::string &state) {
  const size_t sp = state.rfind(' ');
  if (sp != std::string::npos && !state.empty() && state.back() == '%') return state.substr(sp + 1);
  return ui::shortState(state, kEll);
}

// Compact 2x1 (W = 334): F18 name and short state, same buttons (layout.md 2b).
void drawCoverCompact(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const auto v = uistate::coverView(d.members, d.group);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  drawText(x.c, Font::F18, fitText(x.c, Font::F18, title, 83), x.X(12), x.Y(44), Align::Left,
           una ? OFFGREY : INK);
  drawText(x.c, Font::F18, fitText(x.c, Font::F18, compactState(v.state), 83), x.X(12), x.Y(77), Align::Left,
           INK);
  if (una) drawBadge(x, 71, 2, Icon::Warn, OFFGREY);  // UNVERIFIED position (spec silent)
  else if (v.alertBadge || d.failed) drawBadge(x, 71, 2, Icon::Alert, INK);
  auto subs = ui::subRegions(WidgetType::Cover, config::WidgetSize{}, x.r);
  const Icon glyphs[3] = {Icon::Up, Icon::Stop, Icon::Down};
  const bool filled[3] = {v.upFilled, false, v.downFilled};
  for (int i = 0; i < 3; ++i)
    drawButton(x, subs[size_t(i)].rect, glyphs[i], filled[i], d.pending && d.pendingSub == i + 1, una,
               x.innerSub == i + 1);
  (void)W;
}

void drawCover(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  if (x.W() < kCompactW) { drawCoverCompact(x, d, title); return; }
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
  Icon icon = Icon::Bulb;
  switch (type) {
    case WidgetType::Light: icon = v.on ? Icon::Bulb : Icon::BulbOff; break;
    case WidgetType::Switch: icon = Icon::Plug; break;
    case WidgetType::Automation: icon = Icon::Auto; break;
    case WidgetType::BinarySensor: icon = stateIcon(v.icon) == Icon::Count ? Icon::DotOff : stateIcon(v.icon); break;
    default: break;
  }
  const bool binWarn = binary && una;
  const bool showBadge = (una && !binary) || failed || pend;
  const Icon badgeIcon = (una && !binary) ? Icon::Warn : failed ? Icon::Alert : Icon::Hourglass;
  const uint32_t badgeColor = (una && !binary) ? OFFGREY : (inkFill ? PAPER : INK);
  const bool ring = pend && !failed && !(una && !binary);
  const Icon glyph = binWarn ? Icon::Warn : icon;
  const uint32_t glyphColor = binWarn ? OFFGREY : fg;
  if (W < kTinyW) {
    // Portrait 4 columns (118 px): icon above name and state, centred. UNVERIFIED: 40 px icon (the mockup shows 32).
    const auto m = measureOn(x.c);
    drawIcon(x.c, glyph, 40, x.X((W - 40) / 2), x.Y(6), glyphColor);
    const ui::TextFit nm = ui::fitName(m, title, W - 16, kEll);
    drawText(x.c, nm.small ? Font::F18 : Font::F24, nm.text, x.X(W / 2), x.Y(68), Align::Center, fg);
    const ui::TextFit st = ui::fitState(m, v.state, W - 16, kEll, false);
    drawText(x.c, Font::F18, st.text, x.X(W / 2), x.Y(91), Align::Center, sub);
    if (showBadge) drawBadge(x, W - 28, 6, badgeIcon, badgeColor);
    if (ring) drawPendingRing(x, inkFill ? PAPER : INK);
    return;
  }
  if (W < kNarrowW) {
    // Portrait 3 columns (161 px): name F24 -> F18 -> "...", state beside the icon with the same fallback.
    const auto m = measureOn(x.c);
    const ui::TextFit nm = ui::fitName(m, title, W - 24, kEll);
    drawText(x.c, nm.small ? Font::F18 : Font::F24, nm.text, x.X(12), x.Y(38), Align::Left, fg);
    drawIcon(x.c, glyph, 40, x.X(12), x.Y(49), glyphColor);
    const ui::TextFit st = ui::fitState(m, v.state, showBadge ? W - 36 - 8 - 58 : W - 70, kEll, true);
    drawText(x.c, st.small ? Font::F18 : Font::F24, st.text, x.X(58), st.small ? x.Y(75) : x.Y(77), Align::Left, sub);
    if (showBadge) drawBadge(x, W - 36, 57, badgeIcon, badgeColor);
    if (ring) drawPendingRing(x, inkFill ? PAPER : INK);
    return;
  }
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, W - 24), x.X(12), x.Y(38), Align::Left, fg);
  drawIcon(x.c, glyph, 40, x.X(12), x.Y(49), glyphColor);
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, v.state, W - 70), x.X(58), x.Y(77), Align::Left, sub);
  if (showBadge) drawBadge(x, W - 36, 57, badgeIcon, badgeColor);
  if (ring) drawPendingRing(x, inkFill ? PAPER : INK);
}

// ---- climate, wide -------------------------------------------------------------------------------

// Compact 2x1 (W = 334): F18 name, F18 current temperature, F24 target, no humidity (layout.md 2b).
void drawClimateCompact(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::climateView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  drawText(x.c, Font::F18, fitText(x.c, Font::F18, title, 76), x.X(12), x.Y(44), Align::Left, una ? OFFGREY : INK);
  drawText(x.c, Font::F18, v.temp, x.X(12), x.Y(77), Align::Left, INK);
  const Icon ai = v.look == Look::Normal ? actionIcon(v.act) : Icon::Count;
  if (ai != Icon::Count) drawIcon(x.c, ai, 24, x.X(64), x.Y(58), INK);
  if (una) drawBadge(x, 64, 2, Icon::Warn, OFFGREY);  // UNVERIFIED position (spec silent)
  else if (d.failed) drawBadge(x, 64, 2, Icon::Alert, INK);
  else if (d.pending) drawBadge(x, 64, 2, Icon::Hourglass, INK);
  auto subs = ui::subRegions(WidgetType::Climate, config::WidgetSize{}, x.r);
  drawButton(x, subs[0].rect, Icon::Minus, false, d.pending && d.pendingSub == 1, una, x.innerSub == 1, false);
  drawButton(x, subs[1].rect, Icon::Plus, false, d.pending && d.pendingSub == 2, una, x.innerSub == 2, false);
  drawText(x.c, Font::F24, v.target, x.X(208), x.Y(61), Align::Center, INK);
}

void drawClimate(Ctx &x, const uistate::WidgetData &d, const std::string &title) {
  if (x.W() < kCompactW) { drawClimateCompact(x, d, title); return; }
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const auto v = uistate::climateView(st);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  const int Lx = W - 288;
  // Narrow variant (landscape 4 columns, W = 458): the action icon sits at the end of the name line
  // (proposal-landscape-4col.svg: flame at 146,16), the humidity run starts at 84.
  const bool narrow = W < 500;
  const Icon ai = v.look == Look::Normal ? actionIcon(v.act) : Icon::Count;
  const int nameW = narrow ? (ai != Icon::Count ? Lx - 12 - 28 : Lx - 12) : W - 328;
  drawText(x.c, Font::F24, fitText(x.c, Font::F24, title, nameW), x.X(12), x.Y(44), Align::Left, una ? OFFGREY : INK);
  const uint32_t dataColor = INK;
  drawText(x.c, Font::F24, v.temp, x.X(12), x.Y(77), Align::Left, dataColor);
  if (v.look == Look::Normal) {
    if (!v.hum.empty()) {
      const int dropX = narrow ? 84 : 92, humX = narrow ? 112 : 120;
      const int humEnd = humX + textWidth(x.c, Font::F24, v.hum);
      // The drop icon is dropped (text stays) if the run would pass Lx, so the action icon keeps its place.
      if (narrow ? humEnd <= Lx : humEnd + 12 + 24 <= Lx) drawIcon(x.c, Icon::Drop, 24, x.X(dropX), x.Y(57), INK);
      drawText(x.c, Font::F24, v.hum, x.X(humX), x.Y(77), Align::Left, INK);
    }
    if (ai != Icon::Count) {
      if (narrow) drawIcon(x.c, ai, 24, x.X(Lx - 24), x.Y(16), INK);
      else drawIcon(x.c, ai, 24, x.X(186), x.Y(57), INK);
    }
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
  const auto v = uistate::sensorView(st, d.cfg);
  const bool una = v.look == Look::Unavailable;
  drawFrame(x, PAPER, una);
  const int W = x.W();
  const uint32_t vc = INK;
  if (pl.cellW >= 2) {
    // 2x1: name left (up to two lines), value right.
    // Portrait 3/4 columns (W < 400): F40 value, name gets what is left of it. UNVERIFIED.
    if (W < kCompactW) {
      std::string unitN = v.unit;
      const Font vf = Font::F40;
      const int valW = v.textState ? textWidth(x.c, Font::F24, v.value) : runWidth(x.c, vf, v.value, Font::F24, unitN);
      const int nameWn = std::max(40, W - 36 - valW);
      auto ln = wrapText(x.c, Font::F24, title, nameWn, 2);
      if (ln.size() >= 2) {
        drawText(x.c, Font::F24, ln[0], x.X(12), x.Y(44), Align::Left, INK);
        drawText(x.c, Font::F24, ln[1], x.X(12), x.Y(77), Align::Left, INK);
      } else if (!ln.empty()) {
        drawText(x.c, Font::F24, ln[0], x.X(12), x.Y(60), Align::Left, INK);
      }
      if (v.textState) drawText(x.c, Font::F24, v.value, x.X(W - 12), x.Y(60), Align::Right, vc);
      else drawRun(x.c, vf, v.value, Font::F24, unitN, x.X(W - 12), x.Y(66), Align::Right, vc);
      if (una) drawBadge(x, W - 36, 8, Icon::Warn, OFFGREY);
      return;
    }
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
  const bool compact = W < kCompactW;  // 2x1: same rules as the compact cover, no battery
  const Font tf = compact ? Font::F18 : Font::F24;
  drawText(x.c, tf, fitText(x.c, tf, title, compact ? 83 : W - 252), x.X(12), x.Y(44), Align::Left,
           una ? OFFGREY : INK);
  const uint32_t sc = INK;
  const int statusMax = compact ? 83 : W - 284;
  const std::string status = fitText(x.c, tf, compact ? compactState(v.status) : v.status, statusMax);
  drawText(x.c, tf, status, x.X(12), x.Y(77), Align::Left, sc);
  if (v.showBattery && !compact) {
    const int ix = 12 + textWidth(x.c, Font::F24, status) + 12;
    drawIcon(x.c, v.lowBattery ? Icon::BatteryLow : Icon::Batt, 24, x.X(ix), x.Y(57), INK);
    drawText(x.c, Font::F18, std::to_string(v.battery) + "%", x.X(ix + 28), x.Y(75), Align::Left, INK);
  }
  const int bx = compact ? 71 : W - 264, by = compact ? 2 : 57;  // compact position UNVERIFIED (spec silent)
  if (una) drawBadge(x, bx, by, Icon::Warn, OFFGREY);
  else if (v.error || d.failed) drawBadge(x, bx, by, Icon::Alert, INK);
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
    case WidgetType::Weather:
      if (isForecastWidget(data.cfg, pl.rect.w)) drawWeatherForecast(c, pl.rect, data);  // task 104
      else drawWeather(x, pl, data, title);
      break;
    case WidgetType::Vacuum: drawVacuum(x, data, title); break;
  }
}

}  // namespace render
