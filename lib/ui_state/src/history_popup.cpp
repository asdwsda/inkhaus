#include <algorithm>
#include <cmath>
#include "history_popup.h"

#include "history_layout.h"

namespace uistate {

int historyColumns(bool landscape, bool step) {
  return history::graphLayout(step ? history::ChartType::Step : history::ChartType::Line, landscape).columns();
}

bool historyEnabledFor(const config::Widget &w) {
  return (w.type == config::WidgetType::Sensor || w.type == config::WidgetType::BinarySensor) &&
         w.attribute.empty() && w.history.enabled && !w.entity.empty();
}

HistorySpec makeHistorySpec(const config::Widget &w, bool landscape) {
  using config::HistorySource;
  using config::HistoryType;
  HistorySpec s;
  s.entity = w.entity;
  s.hours = w.history.hours;
  s.binary = w.type == config::WidgetType::BinarySensor;
  switch (w.history.type) {
    case HistoryType::Bar: s.chart = HistChart::Bar; break;
    case HistoryType::Step: s.chart = HistChart::Step; break;
    case HistoryType::Line: s.chart = HistChart::Line; break;
    case HistoryType::Auto: s.chart = s.binary ? HistChart::Step : HistChart::Line; break;
  }
  const bool bar = s.chart == HistChart::Bar;
  history::SourceCfg sc = history::SourceCfg::Auto;
  if (w.history.source == HistorySource::History) sc = history::SourceCfg::History;
  else if (w.history.source == HistorySource::Statistics) sc = history::SourceCfg::Statistics;
  s.source = history::chooseSource(sc, s.binary, bar, s.hours);
  if (bar) {
    history::BarPeriodCfg pc = history::BarPeriodCfg::Auto;
    if (w.history.period == config::HistoryPeriod::Hour) pc = history::BarPeriodCfg::Hour;
    else if (w.history.period == config::HistoryPeriod::Day) pc = history::BarPeriodCfg::Day;
    s.period = history::barPeriod(pc, s.hours);
  } else {
    s.period = history::linePeriod(s.hours);
  }
  s.statistic = StatsType::Mean;
  if (w.history.statistic) {
    switch (*w.history.statistic) {
      case config::HistoryStatistic::Mean: s.statistic = StatsType::Mean; break;
      case config::HistoryStatistic::Min: s.statistic = StatsType::Min; break;
      case config::HistoryStatistic::Max: s.statistic = StatsType::Max; break;
      case config::HistoryStatistic::Sum: s.statistic = StatsType::Sum; break;
      case config::HistoryStatistic::Change: s.statistic = StatsType::Change; break;
    }
  }
  s.columns = historyColumns(landscape, s.chart == HistChart::Step);
  history::CacheParams &p = s.params;
  p.entity = s.entity;
  p.hours = s.hours;
  p.type = int(w.history.type);
  p.statistic = w.history.statistic ? int(*w.history.statistic) : -1;
  p.period = int(w.history.period);
  p.source = int(w.history.source);
  p.columns = s.columns;
  p.hasMin = w.history.min.has_value();
  p.hasMax = w.history.max.has_value();
  p.min = w.history.min.value_or(0);
  p.max = w.history.max.value_or(0);
  return s;
}

HistoryLayout historyLayout(settings::Orientation o) {
  HistoryLayout l;
  if (o == settings::Orientation::Landscape) {
    l.panel = {40, 86, 880, 424};
    l.dirty = {40, 86, 886, 430};
    l.close = {840, 102, 64, 64};
  } else {
    l.panel = {16, 212, 508, 512};
    l.dirty = {16, 212, 514, 518};
    l.close = {444, 228, 64, 64};
  }
  return l;
}

HistoryHit historyHitTest(const HistoryLayout &l, ui::Point p) {
  auto in = [&](const ui::Rect &r) { return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h; };
  if (in(l.close)) return HistoryHit::Close;
  return in(l.panel) ? HistoryHit::Panel : HistoryHit::Outside;
}

void HistoryPopup::open(const HistorySpec &spec, int tab, int widget, uint32_t now) {
  spec_ = spec;
  tab_ = tab;
  widget_ = widget;
  state_ = HistState::Loading;
  reason_ = HistReason::None;
  fetchWanted_ = true;
  fetching_ = cancelDue_ = offline_ = false;
  loadingShown_ = hasData_ = dataChanged_ = false;
  openedAt_ = now;
  series_ = history::Series();
  dataAt_ = 0;
}

void HistoryPopup::fail(HistReason r) {
  state_ = HistState::Error;
  reason_ = r;
  hasData_ = false;
  offline_ = false;
}

void HistoryPopup::applyCache(history::Freshness f, const history::Series &s, int64_t fetchedAt) {
  if (state_ != HistState::Loading || f == history::Freshness::Missing) return;
  series_ = s;
  dataAt_ = fetchedAt;
  hasData_ = true;
  if (s.kind == history::Kind::Unsupported) {  // never cached in practice; be safe
    fetchWanted_ = false;
    fail(HistReason::NoGraph);
    return;
  }
  if (f == history::Freshness::Fresh) {
    state_ = HistState::Ready;
    fetchWanted_ = false;
  } else {
    state_ = HistState::Stale;
  }
}

bool HistoryPopup::takeFetch(int64_t nowEpoch, HistoryJob &job) {
  if (!fetchWanted_) return false;
  fetchWanted_ = false;
  if (nowEpoch < 1000000000) {  // clock not set: the window cannot be planned
    if (hasData_) { state_ = HistState::Stale; offline_ = true; }
    else fail(HistReason::NotFetched);
    return false;
  }
  job.entity = spec_.entity;
  job.end = nowEpoch;
  job.start = nowEpoch - int64_t(spec_.hours) * 3600;
  job.columns = spec_.columns;
  job.statistics = spec_.source == history::Source::Statistics;
  job.bar = job.statistics && spec_.chart == HistChart::Bar;
  job.period = spec_.period;
  job.statistic = spec_.statistic;
  if (job.bar) {
    job.bucketSeconds = stats_period_seconds(spec_.period);
    const history::BarWindow bw = history::barWindow(nowEpoch, spec_.hours, job.bucketSeconds);
    job.start = bw.start;
    job.bucketCount = bw.count;
  }
  job.ticket = ++ticket_;
  fetching_ = true;
  return true;
}

namespace {
bool sameColumns(const history::Series &a, const history::Series &b) {
  if (a.kind != b.kind || a.columns.size() != b.columns.size()) return false;
  // The cache stores values quantised to 16 bits over [lo, hi]; a cached series and a fresh one differ by up
  // to one step even when nothing changed. Compare with that step as tolerance.
  float lo = 0, hi = 0, tol = 0;
  float alo = 0, ahi = 0, blo = 0, bhi = 0;
  const bool ra = a.range(alo, ahi), rb = b.range(blo, bhi);
  if (ra && rb) { lo = std::min(alo, blo); hi = std::max(ahi, bhi); tol = (hi - lo) / 65534.0f; }
  for (size_t i = 0; i < a.columns.size(); ++i) {
    const history::Column &x = a.columns[i], &y = b.columns[i];
    if (x.has != y.has) return false;
    if (!x.has) continue;
    if (std::fabs(x.min - y.min) > tol || std::fabs(x.max - y.max) > tol || std::fabs(x.last - y.last) > tol)
      return false;
  }
  return true;
}
}  // namespace

bool HistoryPopup::onFetchDone(int ticket, HistOutcome o, history::Series &&s, int64_t nowEpoch) {
  if (!fetching_ || ticket != ticket_) return false;
  fetching_ = false;
  if (state_ == HistState::Closed) return false;
  dataChanged_ = false;
  switch (o) {
    case HistOutcome::Ok:
      if (s.kind == history::Kind::Unsupported) { fail(HistReason::NoGraph); return true; }
      dataChanged_ = !hasData_ || !sameColumns(series_, s);
      if (dataChanged_) series_ = std::move(s);
      hasData_ = true;
      dataAt_ = nowEpoch;
      offline_ = false;
      state_ = HistState::Ready;
      return true;
    case HistOutcome::TooMany: fail(HistReason::TooMany); return true;
    case HistOutcome::Unavailable: fail(HistReason::Unavailable); return true;
    case HistOutcome::NoStatistics: fail(HistReason::NoStatistics); return true;
    case HistOutcome::Cancelled:
    case HistOutcome::Unreachable:
      if (hasData_) { state_ = HistState::Stale; offline_ = true; }
      else fail(HistReason::Unreachable);
      return true;
  }
  return false;
}

bool HistoryPopup::close() {
  if (state_ == HistState::Closed) return false;
  if (fetching_) cancelDue_ = true;
  fetching_ = false;
  fetchWanted_ = false;
  state_ = HistState::Closed;
  return true;
}

bool HistoryPopup::takeCancel() {
  const bool c = cancelDue_;
  cancelDue_ = false;
  return c;
}

bool HistoryPopup::tick(uint32_t now) {
  if (state_ != HistState::Loading || loadingShown_) return false;
  if (now - openedAt_ < kHistoryLoadingDelayMs) return false;
  loadingShown_ = true;
  return true;
}

HistNote HistoryPopup::note() const {
  switch (state_) {
    case HistState::Closed: return HistNote::None;
    case HistState::Loading: return HistNote::Fetching;
    case HistState::Ready: return HistNote::Updated;
    case HistState::Stale: return offline_ ? HistNote::Cached : HistNote::Updating;
    case HistState::Error:
      if (reason_ == HistReason::TooMany) return HistNote::TooMany;
      return HistNote::None;
  }
  return HistNote::None;
}

}  // namespace uistate
