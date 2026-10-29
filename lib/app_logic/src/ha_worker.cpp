#include "ha_worker.h"

#include <chrono>

#include "scanner.h"

namespace app_logic {

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
    runHistory(cmd, e);
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
