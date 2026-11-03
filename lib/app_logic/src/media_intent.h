#pragma once
// Session events for media popup sends (task 106): media_player.volume_set / volume_mute. Custom calls,
// so the session keeps the 036 behaviour (any fetch confirms).
#include <optional>
#include <string>

#include "session.h"

namespace app_logic {

std::optional<session::Event> mediaVolumeIntent(const std::string &playerId, int pct);
std::optional<session::Event> mediaMuteIntent(const std::string &playerId, bool mute);

}  // namespace app_logic
