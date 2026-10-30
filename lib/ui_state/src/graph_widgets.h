#pragma once
// History graph widgets (`display: graph`, task 071, docs/ui/history-widget.md). Pure model: one entry per graph
// widget of the config with its fetch spec, the last data, and the fetch bookkeeping. Fetches run one at a time and
// only for the tabs that were requested (the visible tab; every tab when kGraphFetchAllTabs). No drawing, no network.
#include <cstdint>
#include <functional>
#include <vector>

#include "cache_store.h"
#include "config_model.h"
#include "history_popup.h"
#include "settings.h"

namespace uistate {

// Scheduled refresh policy (task 071 notes): false = a session fetches the graphs of the visible tab only, true =
// the graphs of every tab. The default is the expected result of the device measurement (1-2 s per graph fetch);
// flip it if the log line `APP: graph_fetch ms=` shows all graphs together under 0.5 s.
constexpr bool kGraphFetchAllTabs = false;

constexpr int kGraphTicketBase = 1000000;  // popup tickets are small; a result with a ticket >= this is a graph's

enum class GraphState { Loading, Ready, Error };
enum class GraphReason { None, TooMany, NoGraph, NoStatistics, Unreachable, Unavailable, NotFetched };

struct GraphEntry {
  int tab = 0, widget = 0;  // index in config.tabs / tab.widgets
  int w = 0, h = 0;         // widget size in px
  HistorySpec spec;
  GraphState state = GraphState::Loading;
  GraphReason reason = GraphReason::None;
  bool hasData = false;
  bool stale = false;  // the last fetch failed and the data shown is older (clock marker)
  history::Series series;
  int64_t dataAt = 0;
  bool wanted = false, fetching = false;
  int ticket = 0;
};

class GraphWidgets {
 public:
  // deviceClass(entityId) -> device_class of the entity if known ("energy" makes bars sum).
  using DeviceClassFn = std::function<std::string(const std::string &)>;
  void rebuild(const config::Config &cfg, settings::Orientation o, const DeviceClassFn &deviceClass = nullptr);

  bool any() const { return !entries_.empty(); }
  bool tabHasGraphs(int tab) const;
  const GraphEntry *find(int tab, int widget) const;
  GraphEntry *findMut(int tab, int widget);
  const std::vector<GraphEntry> &entries() const { return entries_; }

  // A session fetched states: ask for the graphs of `tab` (or of every tab when allTabs). Other tabs' queued
  // requests are dropped (a running fetch finishes). Returns the number of graphs requested.
  int requestSession(int tab, bool allTabs);
  // A tab became visible after the session fetch: same as requestSession(tab, false).
  int requestTab(int tab) { return requestSession(tab, false); }

  // The next fetch to start (one at a time). Without a plausible clock nothing can be planned: those entries
  // fail with NotFetched (or keep their data, stale).
  bool takeJob(int64_t nowEpoch, int32_t utcOffsetS, HistoryJob &job);
  // True while a request waits or a fetch runs.
  bool busy() const;
  // Result of a job from takeJob(). False if the ticket is unknown (late result) or nothing visible changed.
  bool onFetchDone(int ticket, HistOutcome o, history::Series &&s, int64_t nowEpoch);
  // The fetch cannot run (offline, auth error, transport did not start): requested entries fail like Unreachable.
  bool fetchFailed(int tab, bool allTabs);
  // The net session that carried the running fetch is gone (restart, sleep): requests are dropped and a running
  // fetch fails like an unreachable one.
  bool abortFetch();
  // Cache hit for an entry without data (the first frame after a wake shows it before the fetch ends).
  bool applyCache(int tab, int widget, const history::Series &s, int64_t fetchedAt);
  // Demo mode: data without a fetch.
  bool setData(int tab, int widget, history::Series &&s, int64_t at);

  uint32_t cacheHash(const GraphEntry &e) const { return history::paramsHash(e.spec.params); }
  // Visible change since the last call (a result, a failure, a cache hit): the caller redraws.
  bool takeChanged();

 private:
  bool markFailed(GraphEntry &e, bool offline);
  std::vector<GraphEntry> entries_;
  int nextTicket_ = kGraphTicketBase;
  bool changed_ = false;
};

}  // namespace uistate
