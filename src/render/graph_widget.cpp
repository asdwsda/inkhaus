#include "graph_widget.h"

#include "graph_widgets.h"
#include "history_graph.h"
#include "widget_view.h"

namespace render {

namespace {

using uistate::GraphReason;
using uistate::GraphState;

history::ChartType chartOf(uistate::HistChart c) {
  switch (c) {
    case uistate::HistChart::Step: return history::ChartType::Step;
    case uistate::HistChart::Bar: return history::ChartType::Bar;
    default: return history::ChartType::Line;
  }
}

std::string hoursText(int hours) {
  if (hours >= 48 && hours % 24 == 0) return std::to_string(hours / 24) + " days";
  return std::to_string(hours) + " h";
}

struct Value {
  std::string text = "-";
  std::string unit;
  bool word = false;  // on/off state word: no unit, never shrunk below F24
};

Value valueOf(const config::Widget &w, const EntityState *st) {
  Value v;
  if (w.type == config::WidgetType::BinarySensor) {
    const auto t = uistate::toggleView(w.type, st, false);
    if (t.look == uistate::Look::Normal) v.text = t.state;
    v.word = true;
    return v;
  }
  const auto s = uistate::sensorView(st, &w);
  if (s.look != uistate::Look::Normal || s.value.empty()) return v;
  v.text = s.value;
  v.unit = s.unit;
  while (!v.unit.empty() && v.unit[0] == ' ') v.unit.erase(0, 1);
  return v;
}

void drawFrameAxes(M5Canvas &c, const history::GraphLayout &l) {
  if (l.type == history::ChartType::Step) {
    const history::Rect &r = l.plot;
    c.fillRect(r.x, r.y, r.w, 2, INK);
    c.fillRect(r.x, r.bottom() - 2, r.w, 2, INK);
    c.fillRect(r.x, r.y, 2, r.h, INK);
    c.fillRect(r.right() - 2, r.y, 2, r.h, INK);
    return;
  }
  c.fillRect(l.axisLeft.x, l.axisLeft.y, l.axisLeft.w, l.axisLeft.h, INK);
  c.fillRect(l.axisBottom.x, l.axisBottom.y, l.axisBottom.w, l.axisBottom.h, INK);
}

// Icon + title on cy = 108 and a subline on cy = 142 (widget-relative), centred on the plot.
void drawMessage(M5Canvas &c, int ox, int oy, int cx, Icon icon, const std::string &title, const std::string &sub,
                 int maxW) {
  const int tw = textWidth(c, Font::F24, title);
  const int total = (icon == Icon::Count ? 0 : 32) + tw;
  int x = cx - total / 2;
  if (icon != Icon::Count) {
    drawIcon(c, icon, 24, x, oy + 108 - 12, INK);
    x += 32;
  }
  drawTextCy(c, Font::F24, title, x, oy + 108, Align::Left, INK);
  if (!sub.empty()) drawTextCy(c, Font::F18, fitText(c, Font::F18, sub, maxW), cx, oy + 142, Align::Center, INK);
  (void)ox;
}

void reasonText(GraphReason r, Icon &icon, std::string &title, std::string &sub) {
  icon = Icon::Alert;
  switch (r) {
    case GraphReason::TooMany: title = "Too many points"; sub = "Set source: statistics"; break;
    case GraphReason::NoGraph: icon = Icon::Warn; title = "No graph"; sub = "Not for this entity type"; break;
    case GraphReason::NoStatistics: icon = Icon::Warn; title = "No statistics"; sub = "None for this sensor"; break;
    case GraphReason::Unavailable: icon = Icon::Warn; title = "Not available"; sub = "Entity unavailable"; break;
    case GraphReason::NotFetched: title = "Not fetched"; sub = "Clock not set"; break;
    default: title = "HA unreachable"; sub = "No cached graph"; break;
  }
}

}  // namespace

void drawGraphWidget(M5Canvas &c, const ui::Placement &pl, const uistate::AppState &app) {
  const int ox = pl.rect.x, oy = pl.rect.y, W = pl.rect.w, H = pl.rect.h;
  const bool landscape = app.orientation() == settings::Orientation::Landscape;
  const bool compact = H < history::kWidgetFullMinH;
  const auto &tabs = app.config().tabs;
  const int tab = app.shownTab();
  if (tab < 0 || size_t(tab) >= tabs.size() || size_t(pl.index) >= tabs[size_t(tab)].widgets.size()) return;
  const config::Widget &w = tabs[size_t(tab)].widgets[size_t(pl.index)];
  const uistate::GraphEntry *e = app.graphs().find(tab, pl.index);
  const EntityState *st = app.store().get(w.entity);

  drawBox(c, pl.rect, PAPER, INK);
  const std::string title = app.widgetTitle(pl.index);
  Value v = valueOf(w, st);

  const history::ChartType type = e ? chartOf(e->spec.chart) : history::ChartType::Line;
  const history::GraphLayout l =
      history::offsetLayout(history::widgetGraphLayout(type, W, H, landscape), ox, oy);

  // ---- header ----
  if (compact) {
    const int blockW = landscape ? 140 : 200;
    drawTextCy(c, Font::F24, fitText(c, Font::F24, title, blockW), ox + 12, oy + 36, Align::Left, INK);
    const std::string run = v.unit.empty() ? v.text : v.text + " " + v.unit;
    drawTextCy(c, Font::F24, fitText(c, Font::F24, run, blockW), ox + 12, oy + 69, Align::Left, INK);
  } else {
    const int maxW = landscape ? 200 : 330;
    Font vf = Font::F40, uf = Font::F24;
    auto width = [&] { return v.unit.empty() ? textWidth(c, vf, v.text) : runWidth(c, vf, v.text, uf, v.unit); };
    if (width() > maxW && !v.word) {  // STYLE 2: one decimal less, then F40 -> F24
      const size_t dot = v.text.find('.');
      if (dot != std::string::npos) v.text.erase(dot);
    }
    if (width() > maxW) { vf = Font::F24; uf = Font::F18; }
    const int right = ox + W - 12;
    if (v.unit.empty()) drawText(c, vf, v.text, right, oy + 50, Align::Right, INK);
    else drawRun(c, vf, v.text, uf, v.unit, right, oy + 50, Align::Right, INK);
    const int nameW = W - 12 - width() - 12 - 12;
    drawTextCy(c, Font::F24, fitText(c, Font::F24, title, nameW), ox + 12, oy + 36, Align::Left, INK);
  }

  // ---- body ----
  const int cx = l.plot.x + l.plot.w / 2;
  const int msgW = l.plot.w - 16;
  auto message = [&](Icon icon, const std::string &t, const std::string &sub) {
    if (compact) {
      const int tw = textWidth(c, Font::F18, t);
      const int total = 32 + tw;
      const int x = cx - total / 2;
      drawIcon(c, icon, 24, x, oy + 52 - 12, INK);
      drawTextCy(c, Font::F18, t, x + 32, oy + 52, Align::Left, INK);
    } else {
      drawFrameAxes(c, l);
      drawMessage(c, ox, oy, cx, icon, t, sub, msgW);
    }
  };

  if (!e || e->state == GraphState::Loading) {
    message(Icon::Hourglass, "Loading", "Fetching history");
    return;
  }
  if (e->state == GraphState::Error) {
    Icon icon;
    std::string t, sub;
    reasonText(e->reason, icon, t, sub);
    if (compact) t = e->reason == GraphReason::NoGraph ? "No graph" : "No data";
    message(icon, t, sub);
    return;
  }
  const history::ChartType ct = type;
  if (!history::hasContent(ct, e->series)) {
    message(Icon::Minus, "No data", "No readings in " + hoursText(e->spec.hours));
    return;
  }

  HistoryGraphInput in;
  in.type = ct;
  in.landscape = landscape;
  in.layout = &l;
  in.series = &e->series;
  in.decimals = w.decimals ? *w.decimals : 1;
  in.hasMin = w.history.min.has_value();
  in.hasMax = w.history.max.has_value();
  in.fixedMin = w.history.min.value_or(0);
  in.fixedMax = w.history.max.value_or(0);
  if (ct == history::ChartType::Bar) {
    const history::Series &sr = e->series;
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
  if (ct == history::ChartType::Step) {
    EntityState d;
    if (st) d.device_class = st->device_class;
    d.available = true;
    d.state = "on";
    in.onWord = uistate::toggleView(config::WidgetType::BinarySensor, &d, false).state;
    d.state = "off";
    in.offWord = uistate::toggleView(config::WidgetType::BinarySensor, &d, false).state;
  }
  if (!drawHistoryGraph(c, in)) drawFrameAxes(c, l);
  if (e->stale && !compact) drawIcon(c, Icon::Clock, 24, ox + 12, oy + H - 44, INK);
}

}  // namespace render
