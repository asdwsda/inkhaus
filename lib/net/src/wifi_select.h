#pragma once
// Wi-Fi profile selection (task 047). Pure logic: SSIDs in, a profile index out.
#include <string>
#include <vector>

namespace netselect {

constexpr int kMaxProfiles = 4;

// `forced`: 0 = Auto, else a 1-based profile number (settings.wifi_profile). A number beyond the
// list counts as Auto (the profile was removed from secrets.yaml).
// Returns the profile to connect to without scanning, or -1 if Auto needs a scan.
// A scan is skipped for a forced profile, a single profile, and when the first profile in the
// list is the one that connected last time with a usable cache (nothing in range could beat it).
int profileWithoutScan(int profileCount, int forced, int lastConnected, bool lastCacheUsable);

// Auto after a scan: the first profile (list order) whose SSID was seen. If none was seen (scan
// failed, hidden SSID): the last connected profile, else the first. -1 only for an empty list.
// `found` (optional) tells whether a scan hit decided.
int pickAfterScan(const std::vector<std::string> &profileSsids, const std::vector<std::string> &seen,
                  int lastConnected, bool *found = nullptr);

}  // namespace netselect
