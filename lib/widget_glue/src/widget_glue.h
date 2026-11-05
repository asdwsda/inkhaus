#pragma once
// Task 124a: the part shared by calendar_glue, forecast_glue and art_glue (no Arduino, no HA types, native-tested):
// the pending/changed flags of one background request in flight, the "post the first due entity" loop and the
// handling of a request dropped by yielding to a user command (HaError::Cancelled).
#include <cstdint>
#include <string>
#include <vector>

namespace widget_glue {

struct FetchState {
  bool pending = false;  // a request is posted and not answered yet
  bool changed = false;  // set when the shown data changed, cleared by takeChanged
  std::string asking;    // entity of the request in flight

  // A new net session started: the old session's answer is dropped.
  void newSession() { pending = false; }
  bool takeChanged() {
    const bool c = changed;
    changed = false;
    return c;
  }
  // The answer arrived. Returns true when it was a Cancelled one (not a failure): the caller keeps the old data.
  bool finish(bool cancelled) {
    pending = false;
    return cancelled;
  }
};

// Stores with markAsked/unmarkAsked/due (calendar::Store, forecast::Store). Posts the first due entity through
// post(entity) and returns true; one per call, the next follows after the answer.
template <class Store, class Post>
bool postFirstDue(FetchState &s, Store &store, const std::vector<std::string> &ents, int64_t nowEpoch,
                  bool scheduledWake, Post post) {
  for (const std::string &e : ents) {
    if (!store.due(e, nowEpoch, scheduledWake)) continue;
    store.markAsked(e, nowEpoch);  // failed or not: no second try within the wake
    s.asking = e;
    s.pending = true;
    post(e);
    return true;
  }
  return false;
}

// Answer of a store-backed request: a Cancelled one is forgotten (asked again later in the session, old data
// kept); returns true then.
template <class Store>
bool finishStore(FetchState &s, Store &store, bool cancelled) {
  if (!s.finish(cancelled)) return false;
  store.unmarkAsked(s.asking);
  return true;
}

}  // namespace widget_glue
