#include "demo_mode.h"

namespace app_logic {

ConfigSource chooseConfigSource(const settings::Settings &s) {
  return s.demo_mode ? ConfigSource::Embedded : ConfigSource::Card;
}

bool isDemoRun(const settings::Settings &s, bool cfgIsDemo, bool cfgHasConnection) {
  return s.demo_mode || cfgIsDemo || !cfgHasConnection;
}

bool needsConfigReload(const settings::Settings &before, const settings::Settings &after) {
  return before.demo_mode != after.demo_mode;
}

}  // namespace app_logic
