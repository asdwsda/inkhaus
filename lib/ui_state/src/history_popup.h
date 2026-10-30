#pragma once
// Model of the history popup (task 053, docs/ui/history.md). Pure logic with a millisecond clock and the
// wall clock passed in; no drawing, no network. States: Closed, Loading (no cache, fetching), Ready (data
// shown), Stale (cached data shown while a fetch runs, or after it failed), Error (a message instead of a
// graph). The popup has no timeout of its own; closing aborts a running fetch (takeCancel()).
#include <cstdint>
#include <string>

#include "cache_store.h"
#include "collector.h"
#include "config_model.h"
#include "settings.h"
#include "stats_plan.h"
#include "ui_layout.h"

namespace uistate {

constexpr uint32_t kHistoryLoadingDelayMs = 300;  // data within this time: the Loading frame is skipped

enum class HistState { Closed, Loading, Ready, Stale, Error };
enum class HistReason { None, TooMany, NoGraph, NoStatistics, Unreachable, Unavailable, NotFetched };
enum class HistNote { None, Updated, Updating, Fetching, Cached, TooMany };
enum class HistChart { Line, Step, Bar };
enum class HistOutcome { Ok, TooMany, Unreachable, Unavailable, Cancelled, NoStatistics };

// What a popup shows and fetches; built from the widget's config.
struct HistorySpec {
  std::string entity;
  int hours = config::kDefaultHistoryHours;
  HistChart chart = HistChart::Line;
  int columns = 0;
  bool binary = false;           // widget is a binary_sensor
  history::Source source = history::Source::History;  // resolved from the config (task 054 chooseSource)
  StatsPeriod period = StatsPeriod::Hour;               // statistics source only
  StatsType statistic = StatsType::Mean;                // bars: which statistic
  history::CacheParams params;
};
// Plot columns: line/bar = plot width, step = lane width (docs/ui/history.md section 3).
int historyColumns(bool landscape, bool step);
// deviceClass: of the entity if known; "energy" makes `sum` the default statistic of bar charts.
HistorySpec makeHistorySpec(const config::Widget &w, bool landscape, const std::string &deviceClass = "");
// Widgets that open the popup: sensor / binary_sensor, no attribute, history not disabled.
bool historyEnabledFor(const config::Widget &w);

// One fetch to run (epoch seconds).
struct HistoryJob {
  std::string entity;
  int64_t start = 0, end = 0;
  int columns = 0;
  int ticket = 0;  // echoed by onFetchDone(); a result of an older job is ignored
  // Statistics source (task 055): WebSocket recorder/statistics_during_period instead of REST history.
  bool statistics = false;
  bool bar = false;  // bars: bucketCount buckets of bucketSeconds from `start` (aligned), else `columns`
  StatsPeriod period = StatsPeriod::Hour;
  StatsType statistic = StatsType::Mean;
  int64_t bucketSeconds = 0;
  int bucketCount = 0;
};

struct HistoryLayout {
  ui::Rect panel, dirty, close;
};
HistoryLayout historyLayout(settings::Orientation o);
enum class HistoryHit { Outside, Panel, Close };
HistoryHit historyHitTest(const HistoryLayout &l, ui::Point p);

class HistoryPopup {
 public:
  void open(const HistorySpec &spec, int tab, int widget, uint32_t now);
  // After open(): the cache lookup result (Missing = nothing). Fresh = no fetch; Stale = show, then fetch.
  void applyCache(history::Freshness f, const history::Series &s, int64_t fetchedAt);
  // Returns true once if a fetch should start now. Without a plausible clock the fetch cannot be planned:
  // no job, and the popup shows the failure (Error NotFetched, or Stale+offline with a cache).
  bool takeFetch(int64_t nowEpoch, HistoryJob &job, int32_t utcOffsetS = 0);
  // The outcome of the fetch started by takeFetch(). Returns true if the picture changed (always, except
  // for a late result after close). `s` is moved from on Ok.
  bool onFetchDone(int ticket, HistOutcome o, history::Series &&s, int64_t nowEpoch);
  // The transport could not start (no Wi-Fi, no session): same as an Unreachable outcome.
  bool fetchUnavailable() { return onFetchDone(ticket_, HistOutcome::Unreachable, history::Series(), 0); }
  // Closes; true if it was open. A running fetch makes takeCancel() true once.
  bool close();
  bool takeCancel();
  // Reveals the Loading frame after kHistoryLoadingDelayMs; true if the picture changed.
  bool tick(uint32_t now);

  bool isOpen() const { return state_ != HistState::Closed; }
  // Drawn at all? Loading stays hidden for the first kHistoryLoadingDelayMs.
  bool visible() const { return isOpen() && (state_ != HistState::Loading || loadingShown_); }
  bool fetching() const { return fetching_; }
  bool offline() const { return offline_; }
  HistState state() const { return state_; }
  HistReason reason() const { return reason_; }
  HistNote note() const;
  const HistorySpec &spec() const { return spec_; }
  int tab() const { return tab_; }
  int widget() const { return widget_; }
  bool hasData() const { return hasData_; }
  const history::Series &series() const { return series_; }
  int64_t dataEpoch() const { return dataAt_; }    // when the shown data was fetched
  bool dataChanged() const { return dataChanged_; }  // the last result differs from what was shown
  const std::string &entity() const { return spec_.entity; }

 private:
  void fail(HistReason r);
  HistState state_ = HistState::Closed;
  HistReason reason_ = HistReason::None;
  HistorySpec spec_;
  int tab_ = 0, widget_ = 0;
  bool fetchWanted_ = false, fetching_ = false, cancelDue_ = false, offline_ = false;
  bool loadingShown_ = false, hasData_ = false, dataChanged_ = false;
  uint32_t openedAt_ = 0;
  int ticket_ = 0;
  history::Series series_;
  int64_t dataAt_ = 0;
};

}  // namespace uistate
