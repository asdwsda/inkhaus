#include "graph_widgets.h"

#include "history_layout.h"
#include "ui_layout.h"

namespace uistate {

namespace {
history::ChartType chartType(HistChart c) {
  switch (c) {
    case HistChart::Step: return history::ChartType::Step;
    case HistChart::Bar: return history::ChartType::Bar;
    default: return history::ChartType::Line;
  }
}
}  // namespace

void GraphWidgets::rebuild(const config::Config &cfg, settings::Orientation o, const DeviceClassFn &deviceClass) {
  entries_.clear();
  changed_ = false;
  const bool landscape = o == settings::Orientation::Landscape;
  for (size_t t = 0; t < cfg.tabs.size(); ++t) {
    const config::Tab &tab = cfg.tabs[t];
    bool any = false;
    for (const config::Widget &w : tab.widgets) any |= config::isGraphWidget(w);
    if (!any) continue;
    const ui::Flow flow = ui::computeFlow(o, tab.widgets, tab.landscapeColumns);
    for (const ui::Placement &p : flow.placements) {
      const config::Widget &w = tab.widgets[size_t(p.index)];
      if (!config::isGraphWidget(w)) continue;
      GraphEntry e;
      e.tab = int(t);
      e.widget = p.index;
      e.w = p.rect.w;
      e.h = p.rect.h;
      e.spec = makeHistorySpec(w, landscape, deviceClass ? deviceClass(w.entity) : std::string());
      e.spec.columns = history::widgetGraphLayout(chartType(e.spec.chart), e.w, e.h, landscape).columns();
      e.spec.params.columns = e.spec.columns;
      entries_.push_back(std::move(e));
    }
  }
}

bool GraphWidgets::tabHasGraphs(int tab) const {
  for (const GraphEntry &e : entries_)
    if (e.tab == tab) return true;
  return false;
}

const GraphEntry *GraphWidgets::find(int tab, int widget) const {
  for (const GraphEntry &e : entries_)
    if (e.tab == tab && e.widget == widget) return &e;
  return nullptr;
}

GraphEntry *GraphWidgets::findMut(int tab, int widget) {
  for (GraphEntry &e : entries_)
    if (e.tab == tab && e.widget == widget) return &e;
  return nullptr;
}

int GraphWidgets::requestSession(int tab, bool allTabs) {
  int n = 0;
  for (GraphEntry &e : entries_) {
    if (e.fetching) continue;
    e.wanted = allTabs || e.tab == tab;
    n += e.wanted;
  }
  return n;
}

bool GraphWidgets::busy() const {
  for (const GraphEntry &e : entries_)
    if (e.wanted || e.fetching) return true;
  return false;
}

bool GraphWidgets::takeJob(int64_t nowEpoch, int32_t utcOffsetS, HistoryJob &job) {
  for (const GraphEntry &e : entries_)
    if (e.fetching) return false;
  for (GraphEntry &e : entries_) {
    if (!e.wanted) continue;
    e.wanted = false;
    if (nowEpoch < 1000000000) {  // clock not set: the window cannot be planned
      markFailed(e, false);
      changed_ = true;
      continue;
    }
    planHistoryJob(e.spec, nowEpoch, utcOffsetS, job);
    job.ticket = e.ticket = ++nextTicket_;
    e.fetching = true;
    return true;
  }
  return false;
}

bool GraphWidgets::markFailed(GraphEntry &e, bool offline) {
  const GraphState s0 = e.state;
  const GraphReason r0 = e.reason;
  const bool st0 = e.stale;
  if (e.hasData) {
    e.stale = true;
    e.state = GraphState::Ready;
  } else {
    e.state = GraphState::Error;
    e.reason = offline ? GraphReason::Unreachable : GraphReason::NotFetched;
  }
  return s0 != e.state || r0 != e.reason || st0 != e.stale;
}

bool GraphWidgets::onFetchDone(int ticket, HistOutcome o, history::Series &&s, int64_t nowEpoch) {
  GraphEntry *e = nullptr;
  for (GraphEntry &x : entries_)
    if (x.fetching && x.ticket == ticket) e = &x;
  if (!e) return false;
  e->fetching = false;
  bool changed = false;
  auto fail = [&](GraphReason r) {
    changed = e->state != GraphState::Error || e->reason != r || e->hasData;
    e->state = GraphState::Error;
    e->reason = r;
    e->hasData = false;
    e->stale = false;
    e->series = history::Series();
  };
  switch (o) {
    case HistOutcome::Ok:
      if (s.kind == history::Kind::Unsupported) {
        fail(GraphReason::NoGraph);
        break;
      }
      changed = !e->hasData || e->state != GraphState::Ready || e->stale || !sameSeriesData(e->series, s);
      if (!e->hasData || !sameSeriesData(e->series, s)) e->series = std::move(s);
      e->hasData = true;
      e->stale = false;
      e->state = GraphState::Ready;
      e->reason = GraphReason::None;
      e->dataAt = nowEpoch;
      break;
    case HistOutcome::TooMany: fail(GraphReason::TooMany); break;
    case HistOutcome::Unavailable: fail(GraphReason::Unavailable); break;
    case HistOutcome::NoStatistics: fail(GraphReason::NoStatistics); break;
    case HistOutcome::Cancelled:
    case HistOutcome::Unreachable: changed = markFailed(*e, true); break;
  }
  if (changed) changed_ = true;
  return changed;
}

bool GraphWidgets::fetchFailed(int tab, bool allTabs) {
  bool changed = false;
  for (GraphEntry &e : entries_) {
    e.wanted = false;
    if (!allTabs && e.tab != tab) continue;
    if (e.fetching) continue;
    changed |= markFailed(e, true);
  }
  if (changed) changed_ = true;
  return changed;
}

bool GraphWidgets::abortFetch() {
  bool changed = false;
  for (GraphEntry &e : entries_) {
    e.wanted = false;
    if (!e.fetching) continue;
    e.fetching = false;
    changed |= markFailed(e, true);
  }
  if (changed) changed_ = true;
  return changed;
}

bool GraphWidgets::applyCache(int tab, int widget, const history::Series &s, int64_t fetchedAt) {
  GraphEntry *e = findMut(tab, widget);
  if (!e || e->hasData || s.kind == history::Kind::Unsupported) return false;
  e->series = s;
  e->hasData = true;
  e->dataAt = fetchedAt;
  e->state = GraphState::Ready;
  e->reason = GraphReason::None;
  changed_ = true;
  return true;
}

bool GraphWidgets::setData(int tab, int widget, history::Series &&s, int64_t at) {
  GraphEntry *e = findMut(tab, widget);
  if (!e) return false;
  e->series = std::move(s);
  e->hasData = true;
  e->stale = false;
  e->dataAt = at;
  e->state = GraphState::Ready;
  e->reason = GraphReason::None;
  changed_ = true;
  return true;
}

bool GraphWidgets::takeChanged() {
  const bool c = changed_;
  changed_ = false;
  return c;
}

}  // namespace uistate
