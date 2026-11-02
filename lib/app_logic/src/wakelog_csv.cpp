#include "wakelog_csv.h"

namespace app_logic {

namespace {

std::string clean(const std::string &s) {
  std::string o;
  for (char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                    c == '.' || c == '-';
    o += ok ? c : '_';
  }
  return o;
}

std::string num(int64_t v) { return v < 0 ? std::string() : std::to_string((long long)v); }

}  // namespace

std::string wakeLogHeader() {
  return "epoch,wake,session,battery_mv,awake_ms,display_ms,wifi_ms,fetch_ms,refresh_ms,fetch_result,refreshes,reset_reason,wake_cause,ext1_mask,rtc_boot_count,slept_path,hist_n,hist_ms,hist_bytes\n";
}

std::string wakeLogLine(const WakeRecord &r) {
  std::string o;
  o += std::to_string((long long)r.epoch) + ',' + clean(r.wake) + ',' + clean(r.session) + ',' +
       std::to_string((long long)r.battery_mv) + ',' + num(r.awake_ms) + ',' + num(r.display_ms) + ',' +
       num(r.wifi_ms) + ',' + num(r.fetch_ms) + ',' + num(r.refresh_ms) + ',' + clean(r.fetch_result) + ',' +
       std::to_string((long long)r.refreshes) + ',' + clean(r.reset_reason) + ',' + clean(r.wake_cause) + ',' +
       clean(r.ext1_mask) + ',' + std::to_string((long long)r.rtc_boot_count) + ',' + clean(r.slept_path) + ',' + std::to_string((long long)r.hist_n) + ',' +
       std::to_string((long long)r.hist_ms) + ',' + std::to_string((long long)r.hist_bytes) + '\n';
  return o;
}

}  // namespace app_logic
