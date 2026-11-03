#include "weather_forecast_draw.h"

#include <cmath>

#include "forecast_view.h"
#include "text_fit.h"

namespace render {

using uistate::Look;
using uistate::WeatherIcon;

namespace {

// Slanted 2 px (scaled) stroke, UNVERIFIED look on the panel.
void stroke(M5Canvas &c, float x0, float y0, float x1, float y1, float s, uint32_t color) {
  const int t = std::max(2, int(std::lround(2.0f * s)));
  for (int i = 0; i < t; ++i)
    c.drawLine(int(std::lround(x0 * s)) + i - t / 2, int(std::lround(y0 * s)), int(std::lround(x1 * s)) + i - t / 2,
               int(std::lround(y1 * s)), color);
}

void ring(M5Canvas &c, float cx, float cy, float r, float s, uint32_t color) {
  const int ro = int(std::lround(r * s)), ri = std::max(0, ro - std::max(2, int(std::lround(2.0f * s))));
  c.fillCircle(int(std::lround(cx * s)), int(std::lround(cy * s)), ro, color);
  c.fillCircle(int(std::lround(cx * s)), int(std::lround(cy * s)), ri, PAPER);
}

// Weather glyphs that have no bitmap yet (pouring, windy): drawn per STYLE.md (stroke 2 on the 24 grid, scaled).
void drawHandGlyph(M5Canvas &c, WeatherIcon w, int size, int x, int y, uint32_t color) {
  const float s = float(size) / 24.0f;
  if (w == WeatherIcon::Pouring) {
    drawIcon(c, Icon::Rainy, size, x, y, color);  // cloud + 3 drops, plus two more drops between them
    c.setClipRect(x, y, size, size);
    auto at = [&](float px, float py) { return std::pair<float, float>{float(x) / s + px, float(y) / s + py}; };
    const auto a = at(10.2f, 19.5f), b = at(9.4f, 23.0f), d = at(14.7f, 19.5f), e = at(13.9f, 23.0f);
    stroke(c, a.first, a.second, b.first, b.second, s, color);
    stroke(c, d.first, d.second, e.first, e.second, s, color);
    c.clearClipRect();
    return;
  }
  // Windy: three horizontal strokes, curls at the ends of the first and the last.
  const float ox = float(x) / s, oy = float(y) / s;
  stroke(c, ox + 3, oy + 9, ox + 14, oy + 9, s, color);
  ring(c, ox + 16.2f, oy + 6.5f, 3.2f, s, color);
  stroke(c, ox + 3, oy + 13, ox + 20, oy + 13, s, color);
  stroke(c, ox + 3, oy + 17, ox + 11, oy + 17, s, color);
  ring(c, ox + 13.2f, oy + 19.5f, 3.2f, s, color);
}

Icon bitmapIcon(WeatherIcon w) {
  switch (w) {
    case WeatherIcon::Sunny: return Icon::Sunny;
    case WeatherIcon::ClearNight: return Icon::Moon;
    case WeatherIcon::PartlyCloudy: return Icon::Partlycloudy;
    case WeatherIcon::Cloudy: return Icon::Cloud;
    case WeatherIcon::Rainy: return Icon::Rainy;
    case WeatherIcon::Snowy: return Icon::Snowy;
    case WeatherIcon::Fog: return Icon::Fog;
    case WeatherIcon::Lightning: return Icon::Lightning;
    default: return Icon::Unknown;
  }
}

void drawWeatherGlyph(M5Canvas &c, WeatherIcon w, int size, int x, int y, uint32_t color) {
  if (w == WeatherIcon::Pouring || w == WeatherIcon::Windy) drawHandGlyph(c, w, size, x, y, color);
  else drawIcon(c, bitmapIcon(w), size, x, y, color);
}

}  // namespace

bool isForecastWidget(const config::Widget *w, int widthPx) {
  return w && w->forecast && w->type == config::WidgetType::Weather && widthPx >= ui::kForecastMinW;
}

void drawWeatherForecast(M5Canvas &c, const ui::Rect &r, const uistate::WidgetData &d) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const forecast::ForecastView v = forecast::forecastView(st, d.forecast, d.today);
  const forecast::Layout l = forecast::layoutFor(r.w);
  const bool una = v.now.look == Look::Unavailable;
  const bool loading = v.now.look == Look::Loading;
  auto X = [&](int dx) { return r.x + dx; };
  auto Y = [&](int dy) { return r.y + dy; };

  if (una) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
    drawIcon(c, Icon::Unknown, 48, X(l.iconX), Y(l.iconY), OFFGREY);
    drawRun(c, Font::F40, "n/a", Font::F24, "", X(l.unavailTempRight), Y(l.unavailBaseline), Align::Right, INK);
    drawIcon(c, Icon::Warn, 24, X(l.badgeX), Y(l.badgeY), OFFGREY);
    return;
  }
  drawBox(c, r, PAPER, INK);
  drawWeatherGlyph(c, loading ? WeatherIcon::Unknown : v.now.icon, 48, X(l.iconX), Y(l.iconY), INK);
  drawRun(c, Font::F40, v.now.temp, Font::F24, loading ? "" : v.now.unit, X(l.tempX), Y(l.tempBaseline), Align::Left,
          INK);
  if (!loading)
    drawText(c, Font::F18, fitText(c, Font::F18, v.now.cond, l.condMaxW), X(l.condX), Y(l.condBaseline), Align::Left,
             INK);
  c.fillRect(X(l.divX), Y(l.divY), l.divW, l.divH, OFFGREY);
  for (int i = 0; i < forecast::kDays; ++i) {
    const forecast::ColumnRect &cr = l.cols[i];
    const forecast::ColumnView &cv = v.cols[i];
    drawTextCy(c, Font::F18, cv.day, X(cr.cx), Y(cr.dayCy), Align::Center, INK);
    if (cv.hasIcon) drawWeatherGlyph(c, cv.icon, 24, X(cr.iconX), Y(cr.iconY), INK);
    drawTextCy(c, Font::F18, cv.hilo, X(cr.cx), Y(cr.hiloCy), Align::Center, INK);
  }
}

}  // namespace render
