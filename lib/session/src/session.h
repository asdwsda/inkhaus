#pragma once
// Session state machine for one wake session. Pure logic: no Arduino includes, no I/O.
//
// Driving it: call handle(event, now_ms) for every event and execute the returned commands
// in order. now_ms is a monotonic millisecond clock (millis()); send a Tick event every
// ~50-100 ms. At most one request (Wi-Fi connect, fetch or service call) is in flight at a
// time; the machine emits the next one only after the result event arrived. After
// Command::Sleep nothing else is emitted. ShowStatus/MarkWidgets never draw by themselves;
// the device draws only on Command::Render.
#include <cstdint>
#include <string>
#include <vector>

#include "entity_state.h"
#include "ha_client.h"
#include "power.h"
#include "service_call.h"
#include "settings.h"

namespace session {

enum class Status { Connecting, Online, Offline, AuthError };
// Confirmed also means "clear the pending mark" (e.g. two toggles cancelled out).
enum class Mark { Pending, Confirmed, Failed };

constexpr int64_t kConfirmDelayMs = 500;  // first confirmation fetch after a successful call
constexpr int64_t kRetryDelayMs = 1000;   // between confirmation retries
constexpr int kConfirmRetries = 2;        // extra fetches after the first one

struct Event {
  enum class Type {
    Start,         // wake_reason, battery_mv, settings, epoch_s, utc_offset_s
    WifiConnected,
    WifiFailed,
    FetchDone,     // error, changed_ids, states
    ServiceDone,   // ticket, error
    UserAction,    // ids, action (or custom_call), expected_state
    Input,         // any touch/rocker activity (UserAction counts as input too)
    Tick,
  };
  Type type = Type::Tick;

  // Start
  power::WakeReason wake_reason = power::WakeReason::Unknown;
  int32_t battery_mv = 4000;
  settings::Settings settings;
  int64_t epoch_s = 0;  // wall clock at start (UTC); used only for the RTC alarm
  int32_t utc_offset_s = 0;

  // FetchDone / ServiceDone
  HaError error = HaError::None;
  std::vector<std::string> changed_ids;  // from StateStore::update
  std::vector<EntityState> states;       // fetched states (used to check expectations)
  int ticket = 0;

  // UserAction
  std::vector<std::string> ids;
  Action action = Action::LightToggle;
  bool custom = false;  // true: send custom_call instead of action (e.g. climate target)
  ServiceCall custom_call;
  std::string expected_state;  // state after success ("on", "open"); empty: any fetch confirms

  static Event start(power::WakeReason r, int32_t mv, const settings::Settings& s,
                     int64_t epoch_s = 0, int32_t utc_offset_s = 0);
  static Event wifi(bool ok);
  static Event fetch_done(HaError e, std::vector<std::string> changed = {},
                          std::vector<EntityState> states = {});
  static Event service_done(int ticket, HaError e);
  static Event user_action(std::vector<std::string> ids, Action a, std::string expected = "");
  static Event user_custom(std::vector<std::string> ids, ServiceCall call,
                           std::string expected = "");
  static Event input();
  static Event tick();
};

struct Command {
  enum class Type {
    ConnectWifi,
    FetchStates,  // ids; empty = all entities of the layout
    CallService,  // ticket, ids, action or custom_call; answer with ServiceDone(ticket)
    ShowStatus,   // status
    MarkWidgets,  // ids, mark
    Render,
    Sleep,        // plan, session
  };
  Type type = Type::Render;
  std::vector<std::string> ids;
  int ticket = 0;
  Action action = Action::LightToggle;
  bool custom = false;
  ServiceCall custom_call;
  Status status = Status::Connecting;
  Mark mark = Mark::Pending;
  power::SleepPlan plan;
  power::Session session = power::Session::Interactive;
};

class SessionMachine {
 public:
  std::vector<Command> handle(const Event& e, int64_t now_ms);

  bool sleeping() const { return sleeping_; }
  Status status() const { return status_; }
  power::Session kind() const { return kind_; }
  size_t queued() const { return queue_.size(); }

 private:
  enum class Flight { None, Wifi, FetchAll, FetchConfirm, Call };
  struct Item {  // one queued/in-flight action for one entity
    std::string id;
    Action action = Action::LightToggle;
    bool custom = false;
    ServiceCall custom_call;
    std::string expected;
    int attempts = 0;      // confirmation fetches done so far
    int64_t due_ms = 0;    // next confirmation fetch
  };

  void on_start(const Event& e, int64_t now, std::vector<Command>& out);
  void on_user_action(const Event& e, int64_t now, std::vector<Command>& out);
  void on_fetch_done(const Event& e, int64_t now, std::vector<Command>& out);
  void on_service_done(const Event& e, int64_t now, std::vector<Command>& out);
  void pump(int64_t now, std::vector<Command>& out);
  void fail_terminal(Status st, std::vector<Command>& out);
  void mark(const std::vector<std::string>& ids, Mark m, std::vector<Command>& out);
  void finish(int64_t now, std::vector<Command>& out);
  bool busy() const;

  power::Session kind_ = power::Session::Interactive;
  bool started_ = false;
  bool sleeping_ = false;
  bool online_ = false;      // Wi-Fi connected
  bool dead_ = false;        // offline/auth error: no more requests
  bool fetched_ = false;     // initial full fetch done
  bool render_ = false;      // a render is due at the end of this handle()
  Flight flight_ = Flight::None;
  Status status_ = Status::Connecting;
  settings::Settings settings_;
  int64_t start_ms_ = 0;
  int64_t epoch_s_ = 0;
  int32_t utc_offset_s_ = 0;
  power::IdleTimer idle_;
  int next_ticket_ = 1;
  int ticket_ = 0;
  std::vector<Item> queue_;     // waiting for a call, order of first tap
  std::vector<Item> calling_;   // in the call in flight
  std::vector<Item> confirm_;   // call ok, waiting for the state to match
  std::vector<std::string> confirm_fetching_;
};

}  // namespace session
