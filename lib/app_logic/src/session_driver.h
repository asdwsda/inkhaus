#pragma once
// Executes the session machine's commands with injected parts: Wi-Fi, the network port (a
// FreeRTOS task on the device, an inline worker in tests) and the wall clock. Pure logic, no
// Arduino. The UI loop calls tick() every iteration; nothing here blocks.
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ha_worker.h"
#include "net_msgs.h"
#include "session.h"
#include "state_store.h"

namespace app_logic {

class IWifi {
 public:
  enum class State { Connecting, Connected, Failed };
  virtual ~IWifi() = default;
  virtual void begin() = 0;
  virtual State poll() = 0;
  virtual void powerDown() = 0;
};

struct SleepRequest {
  power::SleepPlan plan;
  power::Session session = power::Session::Interactive;
};

class SessionDriver {
 public:
  using Log = std::function<void(const std::string &)>;  // gets "APP: ..." lines
  // entityIds: all entities of the layout (fetched on an empty id list). store: where fetched
  // states go. wallClock: UTC epoch seconds, <= 0 or implausible when the clock is unset.
  SessionDriver(std::vector<std::string> entityIds, StateStore &store, IWifi &wifi, INetPort &net,
                std::function<int64_t()> wallClock, Log log);

  void start(power::WakeReason reason, int32_t batteryMv, const settings::Settings &s, int32_t utcOffsetS,
             int64_t nowMs);
  void userAction(const session::Event &e, int64_t nowMs);  // UserAction event (also counts as input)
  void input(int64_t nowMs);
  // Returns true if the machine started a full fetch for this request (false: ignored). Then
  // fetchSeq() grows when that fetch's result arrives.
  bool refresh(int64_t nowMs);
  void tick(int64_t nowMs);

  // History jobs (task 053) go through the same net port but are not part of the session machine.
  // The handler gets every HistoryDone event.
  using HistoryHandler = std::function<void(NetEvent &&)>;
  void setHistoryHandler(HistoryHandler h) { historyHandler_ = std::move(h); }
  void postHistory(const NetCommand &cmd) { net_.post(cmd); }

  // Task 071 (graph widgets): called when a full state fetch (wake, scheduled, manual refresh) has ended, with
  // its success, before the session machine reacts (so a sleep that follows can be held back).
  using FetchHook = std::function<void(bool ok)>;
  void setFetchHook(FetchHook h) { fetchHook_ = std::move(h); }
  // While this returns true (and for at most kSleepHoldMaxMs) the machine's Sleep command, including the Wi-Fi
  // power-down, waits: the graph fetches that follow the state fetch still need the connection.
  using SleepHold = std::function<bool()>;
  void setSleepHold(SleepHold h) { sleepHold_ = std::move(h); }
  static constexpr int64_t kSleepHoldMaxMs = 20000;

  // One-shot flags, cleared by the call.
  bool takeRender();
  bool takeSleep(SleepRequest &out);

  session::Status status() const { return status_; }
  bool fetching() const { return fetching_; }  // a full/manual fetch is in flight
  bool sleeping() const { return machine_.sleeping(); }
  bool wifiUp() const { return wifiUp_; }
  int64_t lastOkEpoch() const { return lastOkEpoch_; }  // 0 = never
  size_t lastChanged() const { return lastChanged_; }  // entities changed by the last fetch
  uint32_t fetchSeq() const { return fetchSeq_; }  // counts arrived fetch results
  bool lastFetchFailed() const { return lastFetchFailed_; }
  bool lastLinkExternal() const { return external_; }
  bool sessionDone() const { return sessionDone_; }  // at least one fetch result arrived
  const std::map<std::string, session::Mark> &marks() const { return marks_; }
  session::SessionMachine &machine() { return machine_; }

 private:
  void send(const session::Event &e, int64_t now);
  void execute(const std::vector<session::Command> &cmds, int64_t now);
  void onNet(NetEvent &ev, int64_t now);

  std::vector<std::string> entityIds_;
  StateStore &store_;
  IWifi &wifi_;
  INetPort &net_;
  std::function<int64_t()> wallClock_;
  Log log_;
  HistoryHandler historyHandler_;
  FetchHook fetchHook_;
  SleepHold sleepHold_;
  bool sleepHeld_ = false, releasingSleep_ = false;
  int64_t sleepHeldSince_ = 0;
  session::Command heldSleep_;
  session::SessionMachine machine_;
  session::Status status_ = session::Status::Connecting;
  bool wifiPending_ = false, wifiUp_ = false, fetching_ = false, render_ = false, sleepDue_ = false;
  bool lastFetchFailed_ = false, external_ = false, sessionDone_ = false;
  int64_t lastOkEpoch_ = 0;
  int64_t lastOkMs_ = 0;       // monotonic time of the last OK fetch made while the clock was unset
  bool lastOkPending_ = false;  // lastOkEpoch_ to be derived from lastOkMs_ once the clock is known
  uint32_t fetchSeq_ = 0;
  size_t lastChanged_ = 0;
  int64_t tapMs_ = -1, callSentMs_ = -1, respMs_ = -1, lastNowMs_ = 0;  // timing logs
  bool refreshIssued_ = false;
  SleepRequest sleep_;
  std::map<std::string, session::Mark> marks_;
  std::vector<session::Event> self_;  // events produced while executing commands
};

const char *statusName(session::Status s);

}  // namespace app_logic
