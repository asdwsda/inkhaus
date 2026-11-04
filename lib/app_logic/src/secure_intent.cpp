#include "secure_intent.h"

namespace app_logic {

std::optional<session::Event> alarmIntent(const std::string &panelId, AlarmCmd cmd, const std::string &code) {
  ServiceCall call;
  if (!build_alarm_call({panelId}, cmd, code, call)) return std::nullopt;
  return session::Event::user_custom({panelId}, call);
}

}  // namespace app_logic
