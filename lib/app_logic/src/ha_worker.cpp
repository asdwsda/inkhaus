#include "ha_worker.h"

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
