#include "brightness_intent.h"

namespace app_logic {

std::optional<session::Event> brightnessIntent(const std::string &lightId, int pct) {
  ServiceCall call;
  if (!build_light_brightness({lightId}, pct, call)) return std::nullopt;
  return session::Event::user_custom({lightId}, call, pct <= 0 ? "off" : "on");
}

}  // namespace app_logic
