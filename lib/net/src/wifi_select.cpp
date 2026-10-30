#include "wifi_select.h"

#include <algorithm>

namespace netselect {

int profileWithoutScan(int n, int forced, int last, bool lastCacheUsable) {
  if (n <= 0) return -1;
  if (forced >= 1 && forced <= n) return forced - 1;
  if (n == 1) return 0;
  if (last >= 0 && last < n && lastCacheUsable) return last;
  return -1;
}

int pickAfterScan(const std::vector<std::string> &ssids, const std::vector<std::string> &seen,
                  int last, bool *found, int exclude) {
  if (found) *found = false;
  for (size_t i = 0; i < ssids.size(); ++i) {
    if (int(i) == exclude) continue;
    if (std::find(seen.begin(), seen.end(), ssids[i]) != seen.end()) {
      if (found) *found = true;
      return int(i);
    }
  }
  const int n = int(ssids.size());
  if (n == 0) return -1;
  if (last >= 0 && last < n && last != exclude) return last;
  for (int i = 0; i < n; ++i)
    if (i != exclude) return i;
  return -1;
}

bool scanAfterDirectFailure(int n, int forced, bool direct, bool scanned) {
  if (!direct || scanned || n < 2) return false;
  return !(forced >= 1 && forced <= n);
}

}  // namespace netselect
