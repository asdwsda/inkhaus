#include "fake_actions.h"

#include <algorithm>
#include <cmath>

namespace uistate {

ToggleIntent toggleIntent(config::WidgetType type, const std::vector<const EntityState *> &members) {
  if (type != config::WidgetType::Light && type != config::WidgetType::Switch &&
      type != config::WidgetType::Automation)
    return ToggleIntent::None;
  // Mixed group: any available member on -> turn off; otherwise turn on.
  bool any = false;
  for (auto *m : members) {
    if (!m || !m->available) continue;
    any = true;
    if (m->state == "on") return ToggleIntent::TurnOff;
  }
  return any ? ToggleIntent::TurnOn : ToggleIntent::None;
}

ActionResult fakeAction(config::WidgetType type, int sub, const std::vector<const EntityState *> &members) {
  ActionResult r;
  using T = config::WidgetType;
  switch (type) {
    case T::Light:
    case T::Switch:
    case T::Automation: {
      // The whole group goes to the opposite of the group aggregate (any on -> off).
      const ToggleIntent intent = toggleIntent(type, members);
      if (intent == ToggleIntent::None) return r;
      const bool turnOn = intent == ToggleIntent::TurnOn;
      for (auto *m : members) {
        if (!m || !m->available) continue;
        EntityState s = *m;
        s.state = turnOn ? "on" : "off";
        if (type == T::Light) {
          if (turnOn && !s.brightness) s.brightness = 204;  // 80 %
        }
        r.states.push_back(s);
      }
      r.changed = !r.states.empty();
      return r;
    }
    case T::Cover: {
      if (sub < 1 || sub > 3) return r;
      for (auto *m : members) {
        if (!m || !m->available) continue;
        EntityState s = *m;
        if (sub == 1) s.state = "opening";
        else if (sub == 3) s.state = "closing";
        else s.state = s.current_position.value_or(0) > 0 ? "open" : "closed";
        r.states.push_back(s);
      }
      r.changed = !r.states.empty();
      return r;
    }
    case T::Climate: {
      if ((sub != 1 && sub != 2) || members.empty() || !members[0] || !members[0]->available) return r;
      EntityState s = *members[0];
      if (!s.temperature) return r;
      double step = s.target_temp_step.value_or(0.5);
      if (!(step > 0)) step = 0.5;  // HA's default; also catches NaN
      const double lo = s.min_temp.value_or(5.0), hi = s.max_temp.value_or(30.0);
      double t = *s.temperature + (sub == 2 ? step : -step);
      t = std::round(t / step) * step;
      if (t < lo - 1e-9 || t > hi + 1e-9) return r;
      s.temperature = t;
      r.states.push_back(s);
      r.changed = true;
      return r;
    }
    case T::Vacuum: {
      if (sub < 1 || sub > 3 || members.empty() || !members[0] || !members[0]->available) return r;
      EntityState s = *members[0];
      s.state = sub == 1 ? "cleaning" : sub == 2 ? "paused" : "returning";
      r.states.push_back(s);
      r.changed = true;
      return r;
    }
    default:
      return r;
  }
}

}  // namespace uistate
