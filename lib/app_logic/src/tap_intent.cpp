#include "tap_intent.h"

namespace app_logic {

using T = config::WidgetType;
using A = Action;

std::optional<session::Event> intentFromRequest(T type, int sub, const std::vector<EntityState> &req) {
  if (req.empty()) return std::nullopt;
  std::vector<std::string> ids;
  for (const EntityState &s : req) ids.push_back(s.id);
  switch (type) {
    case T::Light:
    case T::Switch:
    case T::Automation: {
      const bool on = req[0].state == "on";
      const A a = type == T::Light ? (on ? A::LightTurnOn : A::LightTurnOff)
                  : type == T::Switch ? (on ? A::SwitchTurnOn : A::SwitchTurnOff)
                                      : (on ? A::AutomationTurnOn : A::AutomationTurnOff);
      return session::Event::user_action(ids, a, on ? "on" : "off");
    }
    case T::Cover: {
      if (sub < 1 || sub > 3) return std::nullopt;
      return session::Event::user_action(ids, sub == 1 ? A::CoverOpen : sub == 2 ? A::CoverStop : A::CoverClose);
    }
    case T::Vacuum: {
      if (sub < 1 || sub > 3) return std::nullopt;
      ids.resize(1);
      return session::Event::user_action(
          ids, sub == 1 ? A::VacuumStart : sub == 2 ? A::VacuumPause : A::VacuumReturnToBase);
    }
    case T::Climate: {
      const EntityState &s = req[0];
      if (!s.temperature) return std::nullopt;
      ServiceCall call;
      if (!build_climate_set_temperature({s.id}, *s.temperature, s.min_temp.value_or(5.0),
                                         s.max_temp.value_or(30.0), s.target_temp_step.value_or(0.5), call))
        return std::nullopt;
      return session::Event::user_custom({s.id}, call);
    }
    default:
      return std::nullopt;
  }
}

}  // namespace app_logic
