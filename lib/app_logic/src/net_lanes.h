#pragma once
// Task 116: two lanes for the network task. User commands (taps, popups, refresh-now) always run before
// background jobs (graphs, report, notifications, forecast, post-wake fetch). The worker picks the user
// lane first at every job boundary; a running background job yields (cooperatively) while a user command waits.
#include <deque>
#include <mutex>

#include "net_msgs.h"

namespace app_logic {

enum class Lane { User, Background };

inline Lane laneOf(const NetCommand &c) {
  switch (c.type) {
    case NetCommand::Type::Call:
    case NetCommand::Type::NotifDismiss: return Lane::User;
    case NetCommand::Type::Fetch: return c.user ? Lane::User : Lane::Background;
    default: return Lane::Background;
  }
}

// What happens to a background job that yielded to a user command.
enum class YieldPolicy { None, Requeue, Drop };  // None: short job, never yields

inline YieldPolicy yieldPolicyOf(NetCommand::Type t) {
  switch (t) {
    case NetCommand::Type::History:        // the graph still matters: retry after the user command
    case NetCommand::Type::Notifications:
    case NetCommand::Type::Forecast: return YieldPolicy::Requeue;
    case NetCommand::Type::Report: return YieldPolicy::Drop;  // no value later; not retried within the session
    default: return YieldPolicy::None;
  }
}

// Thread-safe two-lane FIFO. pop() takes the user lane first. A requeued job goes to the front of the
// background lane (it was already running, so it keeps its place).
template <class T>
class LaneQueue {
 public:
  void push(Lane lane, T v) {
    std::lock_guard<std::mutex> g(m_);
    (lane == Lane::User ? user_ : bg_).push_back(std::move(v));
  }
  void requeueFront(T v) {
    std::lock_guard<std::mutex> g(m_);
    bg_.push_front(std::move(v));
  }
  bool pop(T &out) {
    std::lock_guard<std::mutex> g(m_);
    std::deque<T> &q = !user_.empty() ? user_ : bg_;
    if (q.empty()) return false;
    out = std::move(q.front());
    q.pop_front();
    return true;
  }
  bool userWaiting() const {
    std::lock_guard<std::mutex> g(m_);
    return !user_.empty();
  }
  size_t size() const {
    std::lock_guard<std::mutex> g(m_);
    return user_.size() + bg_.size();
  }

 private:
  mutable std::mutex m_;
  std::deque<T> user_, bg_;
};

}  // namespace app_logic
