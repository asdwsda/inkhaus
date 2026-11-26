#pragma once
// Which config to use at boot/wake: pure decision, no I/O (src/app_config.cpp does the I/O).
// A flaky SD mount or stat must never replace the user's cached config with the demo (task 129).

namespace config {

enum class SdStat {
  Ok,       // both files statted
  NoCard,   // mount failed (after retries)
  NoFile,   // config.yaml missing
  Problem,  // readable but unusable (too large ...): load from SD so the error is shown
};

enum class SourceAction {
  UseCache,   // cache matches the card's fingerprint
  ReadSd,     // changed (or unusable) file on the card: import it
  KeepCache,  // card unreadable, a cache exists: keep it (stale but real)
  Demo,       // card unreadable and there never was a config
};

inline SourceAction decideSource(SdStat stat, bool cachePresent, bool cacheMatches) {
  switch (stat) {
    case SdStat::Ok: return cachePresent && cacheMatches ? SourceAction::UseCache : SourceAction::ReadSd;
    case SdStat::Problem: return SourceAction::ReadSd;
    case SdStat::NoCard:
    case SdStat::NoFile: return cachePresent ? SourceAction::KeepCache : SourceAction::Demo;
  }
  return SourceAction::ReadSd;
}

// Calls `attempt()` up to `tries` times, `pause()` between failures. True on the first success.
template <class Attempt, class Pause>
bool retry(int tries, Attempt attempt, Pause pause) {
  for (int i = 0; i < tries; ++i) {
    if (attempt()) return true;
    if (i + 1 < tries) pause();
  }
  return false;
}

}  // namespace config
