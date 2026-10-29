#include "popup_history.h"

#include <time.h>

#include <cstdio>

#include "history_popup.h"
#include "widget_view.h"

namespace render {

namespace {

using uistate::HistNote;
using uistate::HistReason;
using uistate::HistState;

struct Geo {
  bool landscape = false;
  ui::Rect panel, close, plot;
  ui::Rect axisLeft, axisBottom;
  int nameX = 0, nameCy = 0, nameMaxW = 0;
  int valueX = 0, valueBaseline = 0;
  int rangeRight = 0, rangeCy1 = 0;
  int noteX = 0, noteCy = 0;
};

Geo geo(settings::Orientation o) {
  Geo g;
  const uistate::HistoryLayout l = uistate::historyLayout(o);
  g.panel = l.panel;
  g.close = l.close;
  if (o == settings::Orientation::Landscape) {
    g.landscape = true;
    g.plot = {116, 186, 764, 240};
    g.axisLeft = {115, 182, 2, 245};
    g.axisBottom = {115, 426, 766, 2};
    g.nameX = 56; g.nameCy = 134; g.nameMaxW = 300;
    g.valueX = 372; g.valueBaseline = 148;
    g.rangeRight = 824; g.rangeCy1 = 121;
    g.noteX = 56; g.noteCy = 482;
  } else {
    g.plot = {92, 372, 392, 264};
    g.axisLeft = {91, 368, 2, 269};
    g.axisBottom = {91, 636, 394, 2};
    g.nameX = 32; g.nameCy = 260; g.nameMaxW = 396;
    g.valueX = 32; g.valueBaseline = 342;
    g.rangeRight = 508; g.rangeCy1 = 315;
    g.noteX = 32; g.noteCy = 700;
  }
  return g;
}

std::string hoursText(int hours) {
  if (hours >= 48 && hours % 24 == 0) return std::to_string(hours / 24) + " d";
  return std::to_string(hours) + " h";
}

std::string hhmm(int64_t epoch) {
  if (epoch < 1000000000) return "--:--";
  const time_t t = time_t(epoch);
  struct tm lt;
  localtime_r(&t, &lt);  // TZ was set from the config
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", lt.tm_hour, lt.tm_min);
  return b;
}

// The current value of the widget's entity: "21.5" + unit, or the state word for on/off sensors.
void drawValue(M5Canvas &c, const uistate::AppState &app, const Geo &g) {
  const uistate::HistoryPopup &h = app.history();
  const auto &tabs = app.config().tabs;
  const EntityState *st = app.store().get(h.entity());
  if (h.tab() < 0 || size_t(h.tab()) >= tabs.size()) return;
  const auto &ws = tabs[size_t(h.tab())].widgets;
  if (h.widget() < 0 || size_t(h.widget()) >= ws.size()) return;
  const config::Widget &w = ws[size_t(h.widget())];
  if (w.type == config::WidgetType::BinarySensor) {
    const auto v = uistate::toggleView(w.type, st, false);
    drawText(c, Font::F40, v.state, g.valueX, g.valueBaseline, Align::Left, INK);
    return;
  }
  const auto v = uistate::sensorView(st, &w);
  std::string unit = v.unit;
  while (!unit.empty() && unit[0] == ' ') unit.erase(0, 1);
  if (v.look != uistate::Look::Normal || v.value.empty()) drawText(c, Font::F40, "-", g.valueX, g.valueBaseline, Align::Left, INK);
  else if (unit.empty()) drawText(c, Font::F40, v.value, g.valueX, g.valueBaseline, Align::Left, INK);
  else drawRun(c, Font::F40, v.value, Font::F24, unit, g.valueX, g.valueBaseline, Align::Left, INK);
}

void drawNote(M5Canvas &c, const uistate::HistoryPopup &h, const Geo &g) {
  std::string text;
  Icon icon = Icon::Count;
  const std::string t = hhmm(h.dataEpoch());
  switch (h.note()) {
    case HistNote::None: return;
    case HistNote::Updated: text = "updated " + t; break;
    case HistNote::Updating: icon = Icon::Hourglass; text = "updating"; break;
    case HistNote::Fetching: icon = Icon::Hourglass; text = "fetching"; break;
    case HistNote::Cached: icon = Icon::Clock; text = "cached " + t + ", offline"; break;
    case HistNote::TooMany: icon = Icon::Alert; text = "stopped at 512 KB"; break;
  }
  int x = g.noteX;
  if (icon != Icon::Count) {
    drawIcon(c, icon, 24, x, g.noteCy - 12, INK);
    x += 32;
  }
  drawTextCy(c, Font::F18, text, x, g.noteCy, Align::Left, INK);
}

void drawAxesFrame(M5Canvas &c, const Geo &g) {
  c.fillRect(g.axisLeft.x, g.axisLeft.y, g.axisLeft.w, g.axisLeft.h, INK);
  c.fillRect(g.axisBottom.x, g.axisBottom.y, g.axisBottom.w, g.axisBottom.h, INK);
}

void drawMessage(M5Canvas &c, const Geo &g, Icon icon, const std::string &title, const std::string &sub) {
  const int cx = g.plot.x + g.plot.w / 2;
  drawIcon(c, icon, 40, cx - 20, g.plot.y + 36 - 20, INK);
  drawTextCy(c, Font::F24, title, cx, g.plot.y + 80, Align::Center, INK);
  drawTextCy(c, Font::F18, fitText(c, Font::F18, sub, g.plot.w - 16), cx, g.plot.y + 108, Align::Center, INK);
}

const char *reasonText(HistReason r) {
  switch (r) {
    case HistReason::NeedsStatistics: return "Bar needs source: statistics";
    case HistReason::NoGraph: return "No graph for this entity type";
    case HistReason::Unreachable: return "HA unreachable";
    case HistReason::Unavailable: return "Entity unavailable";
    default: return "not fetched";
  }
}

}  // namespace

void drawHistoryPopup(M5Canvas &c, const uistate::AppState &app) {
  const uistate::HistoryPopup &h = app.history();
  if (!h.visible()) return;
  const Geo g = geo(app.orientation());

  c.fillRoundRect(g.panel.x + 6, g.panel.y + 6, g.panel.w, g.panel.h, 8, INK);
  c.fillRoundRect(g.panel.x, g.panel.y, g.panel.w, g.panel.h, 8, INK);
  c.fillRoundRect(g.panel.x + 3, g.panel.y + 3, g.panel.w - 6, g.panel.h - 6, 5, PAPER);

  drawTextCy(c, Font::F24, fitText(c, Font::F24, app.historyTitle(), g.nameMaxW), g.nameX, g.nameCy, Align::Left, INK);
  drawBox(c, g.close, PAPER, INK);
  drawIcon(c, Icon::Close, 40, g.close.x + 12, g.close.y + 12, INK);
  drawValue(c, app, g);
  drawNote(c, h, g);

  const bool graph = (h.state() == HistState::Ready || h.state() == HistState::Stale) && h.hasData();
  if (graph && !h.series().empty()) {
    drawTextCy(c, Font::F18, "Last " + hoursText(h.spec().hours), g.rangeRight, g.rangeCy1, Align::Right, INK);
    // TODO 052 hook: draw the graph (axes, grid, line/step/bar, range line 2, legend) from h.series() and
    // h.spec().chart into the plot rect / lane. Until it exists the plot area stays empty.
    drawAxesFrame(c, g);
    return;
  }
  drawAxesFrame(c, g);
  switch (h.state()) {
    case HistState::Loading:
      drawMessage(c, g, Icon::Hourglass, "Loading", "Fetching " + hoursText(h.spec().hours) + " of history");
      break;
    case HistState::Ready:
    case HistState::Stale:  // data without a single point
      drawMessage(c, g, Icon::Minus, "No data", "No readings in the last " + hoursText(h.spec().hours));
      break;
    case HistState::Error:
      if (h.reason() == HistReason::TooMany)
        drawMessage(c, g, Icon::Alert, "Too many points", "Set source: statistics");
      else
        drawMessage(c, g, h.reason() == HistReason::Unreachable ? Icon::Alert : Icon::Warn, "Not available",
                    reasonText(h.reason()));
      break;
    case HistState::Closed: break;
  }
}

}  // namespace render
