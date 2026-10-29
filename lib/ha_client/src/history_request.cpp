#include "history_request.h"

#include <cstdio>

namespace {
std::string pct(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string o;
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      o += static_cast<char>(c);
    } else {
      o += '%';
      o += hex[c >> 4];
      o += hex[c & 15];
    }
  }
  return o;
}

bool valid_entity(const std::string& id) {
  size_t dot = id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size() || id.size() > 255) return false;
  for (unsigned char c : id) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
  }
  return true;
}
}  // namespace

std::string iso_utc(int64_t t) {
  int64_t days = t / 86400, rem = t % 86400;
  if (rem < 0) { rem += 86400; days -= 1; }
  // civil_from_days (H. Hinnant)
  int64_t z = days + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  int64_t doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153;
  int64_t d = doy - (153 * mp + 2) / 5 + 1;
  int64_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) y += 1;
  char buf[40];
  snprintf(buf, sizeof buf, "%04lld-%02lld-%02lldT%02lld:%02lld:%02lld+00:00", (long long)y,
           (long long)m, (long long)d, (long long)(rem / 3600), (long long)(rem % 3600 / 60),
           (long long)(rem % 60));
  return buf;
}

bool build_history_path(const std::string& entity_id, int64_t start_epoch, int64_t end_epoch,
                        std::string& out) {
  if (!valid_entity(entity_id) || end_epoch <= start_epoch || start_epoch < 0) return false;
  out = "/api/history/period/" + pct(iso_utc(start_epoch)) + "?filter_entity_id=" + pct(entity_id) +
        "&end_time=" + pct(iso_utc(end_epoch)) +
        "&minimal_response&no_attributes&significant_changes_only";
  return true;
}
