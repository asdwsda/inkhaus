#include "wifi_select.h"

#include <algorithm>

namespace netselect {

int profileWithoutScan(int n, int forced, int last, bool lastCacheUsable) {
  if (n <= 0) return -1;
  if (forced >= 1 && forced <= n) return forced - 1;
  if (n == 1) return 0;
  if (last == 0 && lastCacheUsable) return 0;
  return -1;
}

int pickAfterScan(const std::vector<std::string> &ssids, const std::vector<std::string> &seen,
                  int last, bool *found) {
  if (found) *found = false;
  for (size_t i = 0; i < ssids.size(); ++i)
    if (std::find(seen.begin(), seen.end(), ssids[i]) != seen.end()) {
      if (found) *found = true;
      return int(i);
    }
  if (ssids.empty()) return -1;
  return (last >= 0 && last < int(ssids.size())) ? last : 0;
}

}  // namespace netselect
