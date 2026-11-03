#include "media_intent.h"

namespace app_logic {

std::optional<session::Event> mediaVolumeIntent(const std::string &playerId, int pct) {
  ServiceCall call;
  if (!build_media_volume_set({playerId}, pct, call)) return std::nullopt;
  return session::Event::user_custom({playerId}, call);
}

std::optional<session::Event> mediaMuteIntent(const std::string &playerId, bool mute) {
  ServiceCall call;
  if (!build_media_volume_mute({playerId}, mute, call)) return std::nullopt;
  return session::Event::user_custom({playerId}, call);
}

}  // namespace app_logic
