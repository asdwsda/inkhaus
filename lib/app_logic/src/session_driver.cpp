#include "session_driver.h"

#include "wall_time.h"

namespace app_logic {

using session::Command;
using session::Event;
using CT = Command::Type;

const char *statusName(session::Status s) {
  switch (s) {
    case session::Status::Connecting: return "connecting";
    case session::Status::Online: return "online";
    case session::Status::Offline: return "offline";
    case session::Status::AuthError: return "auth_error";
  }
  return "?";
}

SessionDriver::SessionDriver(std::vector<std::string> entityIds, StateStore &store, IWifi &wifi,
                             INetPort &net, std::function<int64_t()> wallClock, Log log)
    : entityIds_(std::move(entityIds)), store_(store), wifi_(wifi), net_(net),
      wallClock_(std::move(wallClock)), log_(std::move(log)) {}

void SessionDriver::start(power::WakeReason reason, int32_t mv, const settings::Settings &s,
                          int32_t utcOffsetS, int64_t nowMs) {
  const int64_t epoch = wallClock_ ? wallClock_() : 0;
  if (log_) log_("APP: start wake=" + std::to_string(int(reason)) + " mv=" + std::to_string(mv) +
                 " clock=" + (plausibleEpoch(epoch) ? "ok" : "unset"));
  send(Event::start(reason, mv, s, plausibleEpoch(epoch) ? epoch : 0, utcOffsetS), nowMs);
}

void SessionDriver::userAction(const Event &e, int64_t nowMs) {
  if (tapMs_ < 0) tapMs_ = nowMs;
  if (log_) log_("APP: user_action ids=" + std::to_string(e.ids.size()) + (e.custom ? " custom" : ""));
  send(e, nowMs);
}
void SessionDriver::input(int64_t nowMs) { send(Event::input(), nowMs); }
bool SessionDriver::refresh(int64_t nowMs) {
  if (log_) log_("APP: refresh_requested");
  refreshIssued_ = false;
  send(Event::refresh(), nowMs);
  if (!refreshIssued_ && log_) log_("APP: refresh_ignored");
  return refreshIssued_;
}

void SessionDriver::send(const Event &e, int64_t now) {
  lastNowMs_ = now;
  execute(machine_.handle(e, now), now);
}

void SessionDriver::tick(int64_t now) {
  if (wifiPending_) {
    const IWifi::State st = wifi_.poll();
    if (st == IWifi::State::Connected) {
      wifiPending_ = false;
      wifiUp_ = true;
      if (log_) log_("APP: wifi connected");
      send(Event::wifi(true), now);
    } else if (st == IWifi::State::Failed) {
      wifiPending_ = false;
      if (log_) log_("APP: wifi failed");
      send(Event::wifi(false), now);
    }
  }
  if (lastOkPending_) {
    const int64_t t = wallClock_ ? wallClock_() : 0;
    if (plausibleEpoch(t)) {
      lastOkEpoch_ = t - (now - lastOkMs_) / 1000;
      lastOkPending_ = false;
    }
  }
  NetEvent ev;
  while (net_.poll(ev)) onNet(ev, now);
  send(Event::tick(), now);
}

void SessionDriver::onNet(const NetEvent &ev, int64_t now) {
  if (ev.type == NetEvent::Type::Closed) return;
  if (ev.type == NetEvent::Type::FetchDone) {
    std::vector<std::string> changed;
    if (ev.error == HaError::None) changed = store_.update(ev.states);
    lastChanged_ = changed.size();
    fetching_ = false;
    sessionDone_ = true;
    ++fetchSeq_;
    lastFetchFailed_ = ev.error != HaError::None;
    if (!lastFetchFailed_) {
      external_ = ev.external;
      const int64_t t = wallClock_ ? wallClock_() : 0;
      if (plausibleEpoch(t)) {
        lastOkEpoch_ = t;
        lastOkPending_ = false;
      } else {
        lastOkMs_ = now;  // clock unknown: remember when, derive the epoch once it is set
        lastOkPending_ = true;
      }
    }
    if (log_) log_("APP: fetch_done err=" + std::to_string(int(ev.error)) + " http=" +
                   std::to_string(ev.http_status) + " states=" + std::to_string(ev.states.size()) +
                   " changed=" + std::to_string(changed.size()) + (ev.external ? " link=remote" : " link=local"));
    send(Event::fetch_done(ev.error, std::move(changed), ev.states), now);
  } else if (ev.type == NetEvent::Type::ServiceDone) {
    std::vector<std::string> changed;
    if (ev.error == HaError::None) changed = store_.update(ev.states);
    if (log_) {
      log_("APP: call_done ticket=" + std::to_string(ev.ticket) + " err=" + std::to_string(int(ev.error)) +
           " http=" + std::to_string(ev.http_status) + " states=" + std::to_string(ev.states.size()) +
           " changed=" + std::to_string(changed.size()));
      if (callSentMs_ >= 0)
        log_("APP: timing call_to_response_ms=" + std::to_string(now - callSentMs_) + " http_ms=" +
             std::to_string(ev.ms));
    }
    callSentMs_ = -1;
    respMs_ = now;
    send(Event::service_done(ev.ticket, ev.error, std::move(changed), ev.states), now);
  }
}

void SessionDriver::execute(const std::vector<Command> &cmds, int64_t now) {
  std::vector<Event> followUps;
  for (const Command &c : cmds) {
    switch (c.type) {
      case CT::ConnectWifi:
        if (log_) log_("APP: cmd connect_wifi");
        wifiPending_ = true;
        wifi_.begin();
        break;
      case CT::FetchStates: {
        NetCommand n;
        n.type = NetCommand::Type::Fetch;
        n.ids = c.ids.empty() ? entityIds_ : c.ids;
        fetching_ = c.ids.empty();
        if (c.ids.empty()) refreshIssued_ = true;
        if (log_) log_("APP: cmd fetch ids=" + std::to_string(n.ids.size()));
        net_.post(n);
        break;
      }
      case CT::CallService: {
        NetCommand n;
        n.type = NetCommand::Type::Call;
        n.ticket = c.ticket;
        bool ok = true;
        if (c.custom) n.call = c.custom_call;
        else ok = build_service_call(c.action, c.ids, n.call);
        if (!ok) {
          if (log_) log_("APP: cmd call ticket=" + std::to_string(c.ticket) + " build_failed");
          followUps.push_back(Event::service_done(c.ticket, HaError::Parse));
          break;
        }
        if (log_) {
          std::string ids;
          for (const auto &i : c.ids) ids += (ids.empty() ? "" : ",") + i;
          log_("APP: cmd call ticket=" + std::to_string(c.ticket) + " service=" + n.call.domain + "." +
               n.call.service + " ids=" + ids);
        }
        n.ids = entityIds_;  // every layout entity the response mentions updates the store
        if (log_ && tapMs_ >= 0) log_("APP: timing tap_to_call_ms=" + std::to_string(now - tapMs_));
        tapMs_ = -1;
        callSentMs_ = now;
        net_.post(n);
        break;
      }
      case CT::ShowStatus:
        status_ = c.status;
        if (log_) log_(std::string("APP: cmd status=") + statusName(c.status));
        break;
      case CT::MarkWidgets:
        for (const auto &id : c.ids) {
          if (c.mark == session::Mark::Confirmed) marks_.erase(id);
          else marks_[id] = c.mark;
        }
        if (log_) log_("APP: cmd mark=" + std::to_string(int(c.mark)) + " ids=" + std::to_string(c.ids.size()));
        break;
      case CT::Render:
        render_ = true;
        break;
      case CT::Sleep:
        if (log_) log_("APP: cmd sleep alarm=" + std::string(c.plan.rtc_alarm_at ? "yes" : "no") +
                       " touch_wake=" + (c.plan.touch_wake ? "1" : "0"));
        sleep_.plan = c.plan;
        sleep_.session = c.session;
        sleepDue_ = true;
        {
          NetCommand close;  // the kept-alive HTTPS connection goes before Wi-Fi does
          close.type = NetCommand::Type::Close;
          net_.post(close);
        }
        wifi_.powerDown();
        wifiUp_ = false;
        break;
    }
  }
  for (const Event &e : followUps) send(e, now);
}

bool SessionDriver::takeRender() {
  const bool r = render_;
  render_ = false;
  if (r && respMs_ >= 0 && log_) {
    log_("APP: timing response_to_render_ms=" + std::to_string(lastNowMs_ - respMs_));
  }
  if (r) respMs_ = -1;
  return r;
}

bool SessionDriver::takeSleep(SleepRequest &out) {
  if (!sleepDue_) return false;
  sleepDue_ = false;
  out = sleep_;
  return true;
}

}  // namespace app_logic
