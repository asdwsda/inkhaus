#pragma once
// Which Home Assistant URL to try first (task 047): Settings "HA connection" x the connected
// Wi-Fi profile's ha_url x what was learned for that SSID. Pure logic.
#include <cstdint>

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

}  // namespace app_logic
