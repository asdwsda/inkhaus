#include "ha_worker.h"

namespace app_logic {

bool HaWorker::isExternal(const std::string &base) const {
  if (base.empty()) return false;
  std::string a = base, b = internal_;
  while (!a.empty() && a.back() == '/') a.pop_back();
  while (!b.empty() && b.back() == '/') b.pop_back();
  return a != b;
}

NetEvent HaWorker::run(const NetCommand &cmd) {
  NetEvent e;
  if (cmd.type == NetCommand::Type::Fetch) {
    e.type = NetEvent::Type::FetchDone;
    FetchResult f = client_.fetch_states(cmd.ids);
    e.error = f.result.error;
    e.http_status = f.result.http_status;
    e.external = isExternal(f.result.base_url);
    e.states = std::move(f.states);
  } else {
    e.type = NetEvent::Type::ServiceDone;
    e.ticket = cmd.ticket;
    HaResult r = client_.call_service(cmd.call);
    e.error = r.error;
    e.http_status = r.http_status;
    e.external = isExternal(r.base_url);
  }
  return e;
}

}  // namespace app_logic
