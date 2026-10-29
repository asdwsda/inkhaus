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

}  // namespace app_logic
