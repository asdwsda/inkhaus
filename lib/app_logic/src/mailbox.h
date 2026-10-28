#pragma once
// Small thread-safe FIFO (mutex + deque) for the UI loop <-> network task queues. Never blocks
// beyond the lock; pop() returns false when empty.
#include <deque>
#include <mutex>

namespace app_logic {

template <class T>
class Mailbox {
 public:
  void push(T v) {
    std::lock_guard<std::mutex> g(m_);
    q_.push_back(std::move(v));
  }
  bool pop(T &out) {
    std::lock_guard<std::mutex> g(m_);
    if (q_.empty()) return false;
    out = std::move(q_.front());
    q_.pop_front();
    return true;
  }
  size_t size() const {
    std::lock_guard<std::mutex> g(m_);
    return q_.size();
  }

 private:
  mutable std::mutex m_;
  std::deque<T> q_;
};

}  // namespace app_logic
