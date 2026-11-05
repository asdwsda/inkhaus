#include "ready_state.h"

namespace app_logic {

ReadyView ReadyTracker::step(uint32_t now, const ReadyInputs &in) {
  ReadyView want;
  if (in.interactive && !in.demo) {
    if (in.fetchDone) wakeDone_ = true;
    if (in.offline) {
      want.shown = ReadyShown::Offline;
      want.queued = in.queuedTaps > 0;
    } else if (wakeDone_) {
      want.shown = ReadyShown::None;  // steady state: the classic phases (updated / fetching states)
    } else if (!in.wifiUp) {
      want.shown = ReadyShown::NotReady;
      want.queued = in.queuedTaps > 0;
    } else {
      want.shown = ReadyShown::Ready;
    }
  }
  if (want.shown == ReadyShown::Ready) {
    if (!readySeen_) { readySeen_ = true; readySince_ = now; }
    if (view_.shown != ReadyShown::Ready && now - readySince_ < kReadyCoalesceMs) return view_;  // not drawn yet
  } else {
    readySeen_ = false;
  }
  if (want != view_) {
    view_ = want;
    ++changes_;
  }
  return view_;
}

}  // namespace app_logic
