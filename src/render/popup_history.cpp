#include "popup_history.h"

#include <time.h>

#include <cstdio>

#include "history_graph.h"
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
  int legendRight = 0;
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
    g.noteX = 56; g.noteCy = 482; g.legendRight = 904;
  } else {
    g.plot = {92, 372, 392, 264};
    g.axisLeft = {91, 368, 2, 269};
    g.axisBottom = {91, 636, 394, 2};
    g.nameX = 32; g.nameCy = 260; g.nameMaxW = 396;
    g.valueX = 32; g.valueBaseline = 342;
    g.rangeRight = 508; g.rangeCy1 = 315;
    g.noteX = 32; g.noteCy = 700; g.legendRight = 508;
  }
  return g;
}

std::string hoursText(int hours) {
  if (hours >= 48 && hours % 24 == 0) return std::to_string(hours / 24) + " days";
  return std::to_string(hours) + (hours == 1 ? " hour" : " hours");
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
    case HistReason::NoStatistics: return "No statistics for this sensor";
    case HistReason::NoGraph: return "No graph for this entity type";
    case HistReason::Unreachable: return "HA unreachable";
    case HistReason::Unavailable: return "Entity unavailable";
    default: return "not fetched";
  }
}

const config::Widget *widgetOf(const uistate::AppState &app) {
  const uistate::HistoryPopup &h = app.history();
  const auto &tabs = app.config().tabs;
  if (h.tab() < 0 || size_t(h.tab()) >= tabs.size()) return nullptr;
  const auto &ws = tabs[size_t(h.tab())].widgets;
  if (h.widget() < 0 || size_t(h.widget()) >= ws.size()) return nullptr;
  return &ws[size_t(h.widget())];
}

// "Open" / "Closed", "On" / "Off", ... from the entity's device class (same table as the widget).
void stateWords(const uistate::AppState &app, std::string &on, std::string &off) {
  EntityState e;
  const EntityState *cur = app.store().get(app.history().entity());
  if (cur) e.device_class = cur->device_class;
  e.available = true;
  e.state = "on";
  on = uistate::toggleView(config::WidgetType::BinarySensor, &e, false).state;
  e.state = "off";
  off = uistate::toggleView(config::WidgetType::BinarySensor, &e, false).state;
}

std::string lower(std::string s) {
  for (char &ch : s) ch = char(tolower((unsigned char)ch));
  return s;
}

// Range line 2 (docs/ui/history.md section 3.1).
std::string rangeLine2(const uistate::HistoryPopup &h, int decimals, const std::string &onWord, const std::string &unit) {
  if (h.spec().chart == uistate::HistChart::Step) {
    const history::StepSummary ss = history::stepSummary(h.series());
    const int64_t min = ss.onSeconds / 60;
    std::string t = min >= 120 ? std::to_string(min / 60) + " h " + std::to_string(min % 60) + " min"
                               : std::to_string(min) + " min";
    return std::to_string(ss.episodes) + "x " + lower(onWord) + ", " + t + " total";
  }
  float lo, hi;
  if (h.spec().chart == uistate::HistChart::Bar) {
    const bool total = h.spec().statistic == StatsType::Sum || h.spec().statistic == StatsType::Change;
    return history::barRangeLine(h.series(), total, decimals, unit);
  }
  if (!h.series().range(lo, hi)) return "";
  return "min " + history::formatValue(lo, decimals) + " max " + history::formatValue(hi, decimals);
}

// Legend, right-aligned at g.legendRight: swatch 24 x 12 centred on the note line, then its text.
int legendEntry(M5Canvas &c, int right, int cy, uint32_t fill, bool outline, const std::string &text) {
  const int tw = textWidth(c, Font::F18, text);
  drawTextCy(c, Font::F18, text, right, cy, Align::Right, INK);
  const int sx = right - tw - 8 - 24;
  if (outline) {
    c.fillRect(sx, cy - 6, 24, 12, INK);
    c.fillRect(sx + 2, cy - 4, 20, 8, fill);
  } else {
    c.fillRect(sx, cy - 6, 24, 12, fill);
  }
  return sx;
}

void drawLegend(M5Canvas &c, const uistate::HistoryPopup &h, const Geo &g, const std::string &onWord,
                const std::string &offWord) {
  if (h.spec().chart == uistate::HistChart::Step) {
    const int x = legendEntry(c, g.legendRight, g.noteCy, PAPER, true, offWord);
    legendEntry(c, x - 16, g.noteCy, INK, false, onWord);
  } else if (h.spec().chart == uistate::HistChart::Bar) {
    legendEntry(c, g.legendRight, g.noteCy, HIST_MID, true, "today so far");
  } else {
    legendEntry(c, g.legendRight, g.noteCy, HIST_GRID, false, "min-max");
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
  if (graph && history::hasContent(h.spec().chart == uistate::HistChart::Bar ? history::ChartType::Bar : history::ChartType::Line, h.series())) {
    drawTextCy(c, Font::F18, "Last " + hoursText(h.spec().hours), g.rangeRight, g.rangeCy1, Align::Right, INK);
    const config::Widget *w = widgetOf(app);
    HistoryGraphInput in;
    in.landscape = g.landscape;
    in.series = &h.series();
    in.decimals = w && w->decimals ? *w->decimals : 1;
    if (w) {
      in.hasMin = w->history.min.has_value();
      in.hasMax = w->history.max.has_value();
      in.fixedMin = w->history.min.value_or(0);
      in.fixedMax = w->history.max.value_or(0);
    }
    if (h.spec().chart == uistate::HistChart::Bar) {
      const history::Series &sr = h.series();
      const int n = static_cast<int>(sr.buckets.size());
      for (int i = 0; i < n; ++i) {
        const int64_t bs0 = sr.start + sr.bucketSeconds * i;
        HistoryBar b;
        b.has = sr.buckets[size_t(i)].has;
        b.value = sr.buckets[size_t(i)].last;
        b.partial = i == n - 1;
        b.label = history::barLabel(bs0, sr.bucketSeconds, i == n - 1, localUtcOffset(bs0));
        in.bars.push_back(b);
      }
    }
    switch (h.spec().chart) {
      case uistate::HistChart::Line: in.type = history::ChartType::Line; break;
      case uistate::HistChart::Step: in.type = history::ChartType::Step; break;
      case uistate::HistChart::Bar: in.type = history::ChartType::Bar; break;
    }
    std::string onWord = "Open", offWord = "Closed";
    if (in.type == history::ChartType::Step) stateWords(app, onWord, offWord);
    in.onWord = onWord;
    in.offWord = offWord;
    if (!drawHistoryGraph(c, in)) drawAxesFrame(c, g);
    std::string unit;
    if (const EntityState *st = app.store().get(h.entity())) {
      unit = uistate::sensorView(st, w).unit;
      while (!unit.empty() && unit[0] == ' ') unit.erase(0, 1);
    }
    const std::string l2 = rangeLine2(h, in.decimals, onWord, unit);
    if (!l2.empty()) drawTextCy(c, Font::F18, l2, g.rangeRight, g.rangeCy1 + 26, Align::Right, INK);
    drawLegend(c, h, g, onWord, offWord);
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
