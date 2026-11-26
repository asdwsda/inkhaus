#include "wake_updating.h"

namespace app_logic {

void WakeUpdating::exclude(int tab, int widget) {
  if (excluded(tab, widget) || exCount_ >= kMaxExcluded) return;
  exTab_[exCount_] = tab;
  exWidget_[exCount_++] = widget;
}

bool WakeUpdating::excluded(int tab, int widget) const {
  for (int i = 0; i < exCount_; ++i)
    if (exTab_[i] == tab && exWidget_[i] == widget) return true;
  return false;
}

bool WakeUpdating::step(const ReadyInputs &in, bool fetchFailed) {
  if (end_ != StaleEnd::None) return false;  // latched: once over it never comes back this session
  if (!started_) {
    if (!in.interactive || in.demo || in.offline || in.fetchDone) return false;
    started_ = true;
    stale_ = true;
    ++refreshes_;
    return true;
  }
  if (in.fetchDone) {
    end_ = fetchFailed ? StaleEnd::Timeout : StaleEnd::Fresh;
  } else if (in.offline) {
    end_ = StaleEnd::Timeout;
  } else {
    return true;
  }
  stale_ = false;
  ++refreshes_;
  return false;
}

}  // namespace app_logic
