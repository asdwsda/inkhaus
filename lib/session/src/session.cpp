#include "session.h"

#include <algorithm>

namespace session {

// ---- Event factories ------------------------------------------------------------------

Event Event::start(power::WakeReason r, int32_t mv, const settings::Settings& s,
                   int64_t epoch_s, int32_t utc_offset_s) {
  Event e;
  e.type = Type::Start;
  e.wake_reason = r;
  e.battery_mv = mv;
  e.settings = s;
  e.epoch_s = epoch_s;
  e.utc_offset_s = utc_offset_s;
  return e;
}
Event Event::wifi(bool ok) {
  Event e;
  e.type = ok ? Type::WifiConnected : Type::WifiFailed;
  return e;
}
Event Event::fetch_done(HaError err, std::vector<std::string> changed,
                        std::vector<EntityState> states) {
  Event e;
  e.type = Type::FetchDone;
  e.error = err;
  e.changed_ids = std::move(changed);
  e.states = std::move(states);
  return e;
}
Event Event::service_done(int ticket, HaError err, std::vector<std::string> changed,
                          std::vector<EntityState> states) {
  Event e;
  e.type = Type::ServiceDone;
  e.ticket = ticket;
  e.error = err;
  e.changed_ids = std::move(changed);
  e.states = std::move(states);
  return e;
}
Event Event::user_action(std::vector<std::string> ids, Action a, std::string expected) {
  Event e;
  e.type = Type::UserAction;
  e.ids = std::move(ids);
  e.action = a;
  e.expected_state = std::move(expected);
  return e;
}
Event Event::user_custom(std::vector<std::string> ids, ServiceCall call, std::string expected) {
  Event e;
  e.type = Type::UserAction;
  e.ids = std::move(ids);
  e.custom = true;
  e.custom_call = std::move(call);
  e.expected_state = std::move(expected);
  return e;
}
Event Event::input() {
  Event e;
  e.type = Type::Input;
  return e;
}
Event Event::refresh() {
  Event e;
  e.type = Type::Refresh;
  return e;
}
Event Event::tick() { return Event(); }

// ---- Helpers --------------------------------------------------------------------------

namespace {
bool is_toggle(Action a) { return a == Action::LightToggle || a == Action::SwitchToggle; }

// The explicit counterpart (on <-> off) of an action, or the action itself if it has none.
bool is_opposite(Action a, Action b) {
  auto pair = [&](Action on, Action off) {
    return (a == on && b == off) || (a == off && b == on);
  };
  return pair(Action::LightTurnOn, Action::LightTurnOff) ||
         pair(Action::SwitchTurnOn, Action::SwitchTurnOff) ||
         pair(Action::AutomationTurnOn, Action::AutomationTurnOff);
}

Command make(Command::Type t) {
  Command c;
  c.type = t;
  return c;
}
}  // namespace

// ---- State machine --------------------------------------------------------------------

bool SessionMachine::busy() const {
  return flight_ != Flight::None || !queue_.empty() || !calling_.empty() || !confirm_.empty();
}

void SessionMachine::mark(const std::vector<std::string>& ids, Mark m,
                          std::vector<Command>& out) {
  if (ids.empty()) return;
  Command c = make(Command::Type::MarkWidgets);
  c.ids = ids;
  c.mark = m;
  out.push_back(c);
  render_ = true;
}

void SessionMachine::fail_terminal(Status st, std::vector<Command>& out) {
  dead_ = true;
  flight_ = Flight::None;
  status_ = st;
  Command s = make(Command::Type::ShowStatus);
  s.status = st;
  out.push_back(s);
  std::vector<std::string> ids;
  for (auto* v : {&calling_, &queue_, &confirm_})
    for (const Item& it : *v) ids.push_back(it.id);
  calling_.clear();
  queue_.clear();
  confirm_.clear();
  mark(ids, Mark::Failed, out);
  // Scheduled sessions stay dark unless something is on screen to fix.
  if (kind_ == power::Session::Interactive) render_ = true;
}

void SessionMachine::on_start(const Event& e, int64_t now, std::vector<Command>& out) {
  if (started_) return;
  started_ = true;
  settings_ = e.settings;
  start_ms_ = now;
  epoch_s_ = e.epoch_s;
  utc_offset_s_ = e.utc_offset_s;
  kind_ = power::decide_session(e.wake_reason, e.battery_mv);
  idle_ = power::IdleTimer(settings_.idle_timeout_s);
  idle_.start(0);
  if (kind_ == power::Session::LowBattery) {
    finish(now, out);
    return;
  }
  Command s = make(Command::Type::ShowStatus);
  s.status = Status::Connecting;
  out.push_back(s);
  out.push_back(make(Command::Type::ConnectWifi));
  flight_ = Flight::Wifi;
}

void SessionMachine::on_refresh(int64_t now, std::vector<Command>& out) {
  if (kind_ != power::Session::Interactive) return;
  idle_.on_input((now - start_ms_) / 1000);
  if (dead_ && flight_ == Flight::None) {  // retry from scratch: new Wi-Fi connect, then a full fetch
    dead_ = false;
    online_ = false;
    fetched_ = false;
    status_ = Status::Connecting;
    Command s = make(Command::Type::ShowStatus);
    s.status = Status::Connecting;
    out.push_back(s);
    out.push_back(make(Command::Type::ConnectWifi));
    flight_ = Flight::Wifi;
    render_ = true;
    return;
  }
  if (dead_ || flight_ == Flight::Wifi || flight_ == Flight::FetchAll || !fetched_) return;
  refresh_ = true;  // pump() sends it once the queue is empty
  if (status_ != Status::Connecting) {
    status_ = Status::Connecting;  // the icon shows the dot until the fetch ends
    Command s = make(Command::Type::ShowStatus);
    s.status = Status::Connecting;
    out.push_back(s);
    render_ = true;
  }
}

void SessionMachine::on_user_action(const Event& e, int64_t now, std::vector<Command>& out) {
  if (kind_ != power::Session::Interactive) return;
  idle_.on_input((now - start_ms_) / 1000);
  if (dead_) {  // nothing can be sent any more
    mark(e.ids, Mark::Failed, out);
    return;
  }
  std::vector<std::string> pending, cleared;
  for (const std::string& id : e.ids) {
    // A new action supersedes any confirmation still running for this entity.
    confirm_.erase(std::remove_if(confirm_.begin(), confirm_.end(),
                                  [&](const Item& it) { return it.id == id; }),
                   confirm_.end());
    Item item;
    item.id = id;
    item.action = e.action;
    item.custom = e.custom;
    item.custom_call = e.custom_call;
    item.expected = e.expected_state;
    auto it = std::find_if(queue_.begin(), queue_.end(),
                           [&](const Item& q) { return q.id == id; });
    if (it == queue_.end()) {
      queue_.push_back(item);
      pending.push_back(id);
    } else if (!it->custom && !item.custom &&
               ((is_toggle(it->action) && it->action == item.action) ||
                is_opposite(it->action, item.action))) {
      // Two toggles cancel out; so do turn_on then turn_off (or the reverse): the last
      // explicit action equals the state the user originally saw, so nothing is sent.
      queue_.erase(it);
      cleared.push_back(id);
    } else {
      *it = item;  // last action wins, position of the first tap is kept
      pending.push_back(id);
    }
  }
  mark(pending, Mark::Pending, out);
  mark(cleared, Mark::Confirmed, out);
}

void SessionMachine::on_fetch_done(const Event& e, int64_t now, std::vector<Command>& out) {
  Flight f = flight_;
  if (f != Flight::FetchAll && f != Flight::FetchConfirm) return;
  flight_ = Flight::None;
  if (e.error != HaError::None) {
    fail_terminal(e.error == HaError::Auth ? Status::AuthError : Status::Offline, out);
    return;
  }
  if (!e.changed_ids.empty()) render_ = true;
  if (f == Flight::FetchAll) {
    fetched_ = true;
    // Actions sent before this fetch may already show their result.
    std::vector<std::string> done;
    for (const Item& c : confirm_) {
      if (c.expected.empty()) continue;
      for (const EntityState& s : e.states)
        if (s.id == c.id && s.state == c.expected) done.push_back(c.id);
    }
    confirm_.erase(std::remove_if(confirm_.begin(), confirm_.end(),
                                  [&](const Item& c) {
                                    return std::find(done.begin(), done.end(), c.id) != done.end();
                                  }),
                   confirm_.end());
    mark(done, Mark::Confirmed, out);
    if (status_ != Status::Online) {
      status_ = Status::Online;
      Command s = make(Command::Type::ShowStatus);
      s.status = Status::Online;
      out.push_back(s);
      if (kind_ == power::Session::Interactive) render_ = true;
    }
    return;
  }
  std::vector<std::string> ok, failed;
  for (const std::string& id : confirm_fetching_) {
    auto it = std::find_if(confirm_.begin(), confirm_.end(),
                           [&](const Item& c) { return c.id == id; });
    if (it == confirm_.end()) continue;  // superseded meanwhile
    bool match = it->expected.empty();
    if (!match) {
      for (const EntityState& s : e.states)
        if (s.id == id && s.state == it->expected) match = true;
    }
    if (match) {
      ok.push_back(id);
      confirm_.erase(it);
    } else if (++it->attempts >= 1 + kConfirmRetries) {
      failed.push_back(id);
      confirm_.erase(it);
    } else {
      it->due_ms = now + kRetryDelayMs;
    }
  }
  confirm_fetching_.clear();
  mark(ok, Mark::Confirmed, out);
  mark(failed, Mark::Failed, out);  // the render shows the real (fetched) state
}

void SessionMachine::on_service_done(const Event& e, int64_t now, std::vector<Command>& out) {
  if (flight_ != Flight::Call || e.ticket != ticket_) return;
  flight_ = Flight::None;
  if (e.error == HaError::None) {
    if (!e.changed_ids.empty()) render_ = true;  // the response already changed the store
    std::vector<std::string> confirmed;
    for (Item& it : calling_) {
      // A newer action for the same id is already queued: its expectation wins.
      bool newer = false;
      for (const Item& q : queue_) newer |= q.id == it.id;
      // Dedupe by id: the newest expectation replaces any older one.
      confirm_.erase(std::remove_if(confirm_.begin(), confirm_.end(),
                                    [&](const Item& c) { return c.id == it.id; }),
                     confirm_.end());
      if (newer) continue;
      // The call's response carries the entity: that is the confirmation, no fetch needed.
      bool answered = false;
      for (const EntityState& s : e.states) answered |= s.id == it.id;
      if (answered) {
        confirmed.push_back(it.id);
        continue;
      }
      it.attempts = 0;
      it.due_ms = now + kConfirmDelayMs;
      confirm_.push_back(it);
    }
    calling_.clear();
    mark(confirmed, Mark::Confirmed, out);
    return;
  }
  if (e.error == HaError::Auth || e.error == HaError::Network) {
    fail_terminal(e.error == HaError::Auth ? Status::AuthError : Status::Offline, out);
    return;
  }
  std::vector<std::string> ids;
  for (const Item& it : calling_) ids.push_back(it.id);
  calling_.clear();
  mark(ids, Mark::Failed, out);
}

void SessionMachine::pump(int64_t now, std::vector<Command>& out) {
  if (dead_ || !online_ || flight_ != Flight::None) return;
  if (!queue_.empty()) {
    // The leading run of identical actions goes out as one call (a group).
    const Item head = queue_.front();
    Command c = make(Command::Type::CallService);
    c.ticket = ticket_ = next_ticket_++;
    c.action = head.action;
    c.custom = head.custom;
    c.custom_call = head.custom_call;
    size_t n = 0;
    while (n < queue_.size()) {
      const Item& q = queue_[n];
      if (n > 0 && (head.custom || q.custom || q.action != head.action ||
                    q.expected != head.expected))
        break;
      c.ids.push_back(q.id);
      calling_.push_back(q);
      ++n;
    }
    queue_.erase(queue_.begin(), queue_.begin() + n);
    out.push_back(c);
    flight_ = Flight::Call;
    return;
  }
  if (!fetched_ || refresh_) {  // queued actions went out first; the full fetch follows
    refresh_ = false;
    out.push_back(make(Command::Type::FetchStates));
    flight_ = Flight::FetchAll;
    return;
  }
  Command c = make(Command::Type::FetchStates);
  for (const Item& it : confirm_)
    if (it.due_ms <= now) c.ids.push_back(it.id);
  if (!c.ids.empty()) {
    confirm_fetching_ = c.ids;
    out.push_back(c);
    flight_ = Flight::FetchConfirm;
  }
}

void SessionMachine::finish(int64_t now, std::vector<Command>& out) {
  if (render_) {
    out.push_back(make(Command::Type::Render));
    render_ = false;
  }
  Command c = make(Command::Type::Sleep);
  c.session = kind_;
  c.plan = power::plan_sleep(settings_, kind_, epoch_s_ + (now - start_ms_) / 1000,
                             utc_offset_s_);
  out.push_back(c);
  sleeping_ = true;
}

std::vector<Command> SessionMachine::handle(const Event& e, int64_t now_ms) {
  std::vector<Command> out;
  if (sleeping_) return out;
  if (e.type == Event::Type::Start) {
    on_start(e, now_ms, out);
    return out;
  }
  if (!started_) return out;

  switch (e.type) {
    case Event::Type::WifiConnected:
      if (flight_ == Flight::Wifi) {
        flight_ = Flight::None;
        online_ = true;
      }
      break;
    case Event::Type::WifiFailed:
      if (flight_ == Flight::Wifi) fail_terminal(Status::Offline, out);
      break;
    case Event::Type::FetchDone: on_fetch_done(e, now_ms, out); break;
    case Event::Type::ServiceDone: on_service_done(e, now_ms, out); break;
    case Event::Type::UserAction: on_user_action(e, now_ms, out); break;
    case Event::Type::Input:
      if (kind_ == power::Session::Interactive) idle_.on_input((now_ms - start_ms_) / 1000);
      break;
    case Event::Type::Refresh: on_refresh(now_ms, out); break;
    default: break;
  }

  pump(now_ms, out);

  const int64_t rel_s = (now_ms - start_ms_) / 1000;
  bool sleep_now;
  if (kind_ == power::Session::ScheduledRefresh) {
    // Done once the fetch finished or the session died; the hard cap covers a hang.
    idle_.set_busy(true);
    sleep_now = fetched_ || dead_ || idle_.should_sleep(rel_s);
  } else {
    idle_.set_busy(busy());
    sleep_now = idle_.should_sleep(rel_s);
  }
  if (sleep_now) {
    finish(now_ms, out);
  } else if (render_) {
    out.push_back(make(Command::Type::Render));
    render_ = false;
  }
  return out;
}

}  // namespace session
