#include "ha_worker.h"

#include <chrono>

#include "scanner.h"
#include "stats_parser.h"
#include "stats_request.h"
#include "ws_endpoint.h"

namespace app_logic {

namespace {
std::string trimmed(std::string a) {
  while (!a.empty() && a.back() == '/') a.pop_back();
  return a;
}
bool sameBase(const std::string &a, const std::string &b) { return trimmed(a) == trimmed(b); }
}  // namespace

bool HaWorker::isExternal(const std::string &base) const {
  if (base.empty()) return false;
  std::string a = base, b = internal_;
  while (!a.empty() && a.back() == '/') a.pop_back();
  while (!b.empty() && b.back() == '/') b.pop_back();
  return a != b;
}

void HaWorker::applyPlan(const UrlPlan &plan) {
  const bool ext = plan.first == Side::External && !external_.empty();
  if (plan.exclusive) {
    // A client with a single URL cannot fall back.
    client_.reset(new HaClient(transport_, ext ? external_ : internal_, "", token_));
  } else {
    client_.reset(new HaClient(transport_, internal_, external_, token_));
  }
  client_->set_remembered(ext ? external_ : internal_);
  planExclusive_ = plan.exclusive;
  planExternalFirst_ = ext;
}

Side HaWorker::answeredSide() const {
  const std::string &r = client_->remembered_base();
  if (r.empty()) return Side::None;
  return isExternal(r) ? Side::External : Side::Internal;
}

// Streams the history body into the scanner/collector (bounded RAM, task 050/051). The cancel flag, the
// 512 KB limit and the 15 s deadline are enforced by stream_history. parse_ms = time spent in the scanner.
void HaWorker::runHistory(const NetCommand &cmd, NetEvent &e) {
  using Clock = std::chrono::steady_clock;
  history::Params hp;
  hp.start = cmd.histStart;
  hp.end = cmd.histEnd;
  hp.columns = cmd.histColumns;
  if (hp.end <= hp.start || hp.columns < 1 || hp.columns > history::kMaxColumns) {
    e.error = HaError::Parse;
    return;
  }
  history::Collector col(hp);
  history::Scanner sc(col);
  StreamLimits lim;
  lim.cancel = cmd.histCancel.get();
  Clock::duration parse{};
  size_t bytes = 0;
  const HaResult r = client_->stream_history(
      cmd.histEntity, cmd.histStart, cmd.histEnd, lim,
      [&](const char *d, size_t n) {
        const auto t0 = Clock::now();
        sc.feed(d, n);
        parse += Clock::now() - t0;
        return true;
      },
      &bytes);
  e.error = r.error;
  e.http_status = r.http_status;
  e.external = isExternal(r.base_url);
  e.ms = r.ms;
  e.fetchMs = r.ms;
  e.reused = r.reused;
  e.bytes = bytes;
  e.parseMs = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(parse).count());
  if (r.error != HaError::None) return;
  e.scan = sc.finish();
  e.points = sc.points();
  e.series = col.finish();
  if (e.scan == history::ScanResult::Malformed) e.error = HaError::Parse;  // not a history array
  else if (e.scan == history::ScanResult::Truncated) e.error = HaError::TooLarge;
}

// Base URLs to try for the WebSocket: the one that answered last (or the plan's first), then the other
// unless the plan is exclusive.
std::vector<std::string> HaWorker::wsBases() const {
  std::vector<std::string> v;
  const std::string first = !client_->remembered_base().empty() ? client_->remembered_base()
                            : (planExternalFirst_ && !external_.empty() ? external_ : internal_);
  if (!first.empty()) v.push_back(first);
  if (!planExclusive_) {
    for (const std::string *u : {&internal_, &external_}) {
      if (u->empty()) continue;
      bool dup = false;
      for (const auto &x : v) dup = dup || sameBase(x, *u);
      if (!dup) v.push_back(*u);
    }
  }
  return v;
}

// Statistics over one WebSocket connection (task 055): upgrade, auth, one command, the result streamed into
// the StatsParser. The kept-alive HTTP socket is closed first when the internal heap is short.
void HaWorker::runStatistics(const NetCommand &cmd, NetEvent &e) {
  using Clock = std::chrono::steady_clock;
  e.stats = true;
  if (!ws_ || cmd.histEnd <= cmd.histStart) {
    e.error = ws_ ? HaError::Parse : HaError::Network;
    return;
  }
  history::StatsParams sp;
  sp.layout.start = cmd.histStart;
  sp.layout.end = cmd.histEnd;
  sp.layout.columns = cmd.histColumns < 1 ? 1 : cmd.histColumns;
  sp.bars = cmd.histBar;
  sp.statistic = cmd.histStatistic;
  sp.periodSeconds = stats_period_seconds(cmd.histPeriod);
  if (cmd.histBar) {
    sp.layout.bucketSeconds = cmd.histBucketSeconds;
    sp.layout.bucketCount = cmd.histBucketCount;
  }
  const uint8_t mask = cmd.histBar ? stats_bit(cmd.histStatistic) : kStatsLineTypes;
  ws::Request req;
  req.token = token_;
  req.limits.cancel = cmd.histCancel.get();
  if (!build_statistics_command(1, cmd.histEntity, cmd.histStart, cmd.histEnd, cmd.histPeriod, mask, req.command_json)) {
    e.error = HaError::Parse;
    return;
  }
  e.error = HaError::Network;  // no base URL could be used
  const auto t0 = Clock::now();
  for (const std::string &base : wsBases()) {
    const ws::Endpoint ep = ws::parse_endpoint(base);
    if (!ep.valid) continue;
    if (ws_->freeInternalHeap() < kWsHeapMargin) transport_.close();
    std::unique_ptr<ws::Socket> sock = ws_->open(ep.host, ep.port, ep.tls);
    if (!sock) continue;
    if (cmd.histCancel && *cmd.histCancel) { e.error = HaError::Cancelled; break; }
    for (int i = 0; i < 4; i++) {
      const uint32_t r = sock->random32();
      for (int k = 0; k < 4; k++) req.key[i * 4 + k] = uint8_t(r >> (8 * k));
    }
    req.host = ep.hostHeader;
    history::StatsParser parser(cmd.histEntity, sp);
    Clock::duration parse{};
    const ws::FlowResult fr = ws::run_command(*sock, req, [&](const char *d, size_t n) {
      const auto a = Clock::now();
      parser.feed(d, n);
      parse += Clock::now() - a;
      return true;
    });
    sock.reset();
    e.bytes = fr.bytes;
    e.fetchMs = fr.ms;
    e.ms = fr.ms;
    e.external = isExternal(base);
    e.parseMs = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(parse).count());
    if (!fr.ok()) {
      // A server-side error (unknown command, recorder missing) is an answer: no fallback to the other URL.
      e.error = fr.server_error ? HaError::Parse : fr.error;
      if (fr.error == HaError::Network && fr.bytes == 0) continue;  // try the other base
      break;
    }
    const history::StatsOutcome o = parser.finish();
    e.error = HaError::None;
    if (o == history::StatsOutcome::NoStatistics) e.noStatistics = true;
    else if (o == history::StatsOutcome::Ok) {
      e.series = parser.series();
      e.points = e.series.points;
    } else {
      e.error = HaError::Parse;  // Truncated / Malformed
    }
    break;
  }
  if (e.fetchMs == 0)
    e.fetchMs = uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());
}

NetEvent HaWorker::run(const NetCommand &cmd) {
  NetEvent e;
  if (cmd.type == NetCommand::Type::Fetch) {
    e.type = NetEvent::Type::FetchDone;
    FetchResult f;  // nothing to fetch is a success with 0 states (no request)
    if (!cmd.ids.empty()) f = client_->fetch_states(cmd.ids);
    e.error = f.result.error;
    e.http_status = f.result.http_status;
    e.external = isExternal(f.result.base_url);
    e.states = std::move(f.states);
    e.ms = f.result.ms;
    e.reused = f.result.reused;
  } else if (cmd.type == NetCommand::Type::History) {
    e.type = NetEvent::Type::HistoryDone;
    e.ticket = cmd.ticket;
    if (cmd.histStats) runStatistics(cmd, e);
    else runHistory(cmd, e);
  } else if (cmd.type == NetCommand::Type::Close) {
    transport_.close();
    e.type = NetEvent::Type::Closed;
  } else {
    e.type = NetEvent::Type::ServiceDone;
    e.ticket = cmd.ticket;
    HaResult r = client_->call_service(cmd.call, &cmd.ids, &e.states);
    e.error = r.error;
    e.http_status = r.http_status;
    e.external = isExternal(r.base_url);
    e.ms = r.ms;
    e.reused = r.reused;
  }
  return e;
}

}  // namespace app_logic
