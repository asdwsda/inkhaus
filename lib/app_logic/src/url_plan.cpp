#include "url_plan.h"

namespace app_logic {

UrlPlan planUrls(LinkMode setting, LinkMode profile, Side remembered, bool haveExternal) {
  UrlPlan p;
  if (!haveExternal) {
    p.first = Side::Internal;
    p.exclusive = true;
    return p;
  }
  if (setting != LinkMode::Auto) {  // forced in Settings: only that URL
    p.first = setting == LinkMode::External ? Side::External : Side::Internal;
    p.exclusive = true;
    return p;
  }
  if (profile != LinkMode::Auto) {  // fixed by the profile: preferred, the other one as fallback
    p.first = profile == LinkMode::External ? Side::External : Side::Internal;
    return p;
  }
  p.first = remembered == Side::External ? Side::External : Side::Internal;
  p.learn = true;
  return p;
}

Side sideToStore(const UrlPlan &plan, Side answered, Side stored) {
  if (!plan.learn || answered == Side::None || answered == stored) return Side::None;
  return answered;
}

bool learnedStale(const Learned &l, int64_t nowEpoch, uint32_t sessionNo) {
  if (l.side != Side::External) return false;
  if (plausibleEpoch(nowEpoch) && plausibleEpoch(l.epoch)) {
    const int64_t age = nowEpoch - l.epoch;
    return age < 0 || age >= kReprobeSeconds;  // a clock that went backwards: re-probe, not trust
  }
  return sessionNo - l.session >= kReprobeSessions;  // unsigned: a reset counter also counts as old
}

Side effectiveRemembered(const Learned &l, int64_t nowEpoch, uint32_t sessionNo) {
  return learnedStale(l, nowEpoch, sessionNo) ? Side::None : l.side;
}

bool shouldStoreLearned(const UrlPlan &plan, Side answered, const Learned &stored, bool stale) {
  if (!plan.learn || answered == Side::None) return false;
  return answered != stored.side || (answered == Side::External && stale);
}

}  // namespace app_logic
