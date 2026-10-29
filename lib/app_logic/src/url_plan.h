#pragma once
// Which Home Assistant URL to try first (task 047): Settings "HA connection" x the connected
// Wi-Fi profile's ha_url x what was learned for that SSID. Pure logic.
#include <cstdint>

#include "wall_time.h"

namespace app_logic {

enum class LinkMode : uint8_t { Auto = 0, Internal = 1, External = 2 };  // setting and profile ha_url
enum class Side : uint8_t { None = 0, Internal = 1, External = 2 };      // stored per SSID

struct UrlPlan {
  Side first = Side::Internal;  // tried first (never None)
  bool exclusive = false;       // only this one, no fallback to the other URL
  bool learn = false;           // persist the side that answered, per SSID
};

// `remembered`: the side stored for the connected SSID (None = nothing yet).
// `haveExternal`: an external_url is configured; without one everything is internal.
UrlPlan planUrls(LinkMode setting, LinkMode profile, Side remembered, bool haveExternal);

// What to store after a request: the side that answered when the plan learns and it differs
// from `stored`; Side::None = nothing to write.
Side sideToStore(const UrlPlan &plan, Side answered, Side stored);

// ---- Re-probe of a learned "external" (task 047) ----
// In Auto/Auto the side that answered is stored per SSID. A learned External must not stick forever (one
// internal timeout at home would keep the device on external): once it is older than kReprobeSeconds, or
// (no clock now) kReprobeSessions sessions old, internal is tried first again. The session counter only
// advances while the clock is not plausible (NVS wear); an External learned without a clock, read while the
// clock is valid, counts as stale at once.
constexpr int64_t kReprobeSeconds = 24 * 3600;
constexpr uint32_t kReprobeSessions = 20;

struct Learned {
  Side side = Side::None;
  int64_t epoch = 0;     // wall clock when learned; 0 = no plausible clock then
  uint32_t session = 0;  // persistent session counter when learned
};

// True: `l` is a learned External that is due for a re-probe. `nowEpoch` 0 / implausible = no clock now.
bool learnedStale(const Learned &l, int64_t nowEpoch, uint32_t sessionNo);

// What planUrls() should see as "remembered": a stale External counts as nothing learned.
Side effectiveRemembered(const Learned &l, int64_t nowEpoch, uint32_t sessionNo);

// Persist {answered, now, sessionNo}? Yes when the plan learns and the answer differs from the stored side, or
// External answered again although the stored one is stale (that renews it, so the re-probe happens once).
bool shouldStoreLearned(const UrlPlan &plan, Side answered, const Learned &stored, bool stale);

}  // namespace app_logic
