#include "stats_request.h"

#include <cstdio>

#include "history_request.h"

std::string json_quote(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}

namespace {
bool valid_entity(const std::string& id) {
  size_t dot = id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size() || id.size() > 255) return false;
  for (unsigned char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
  return true;
}
}  // namespace

bool build_statistics_command(int id, const std::string& entity_id, int64_t start_epoch, int64_t end_epoch,
                              StatsPeriod period, uint8_t types_mask, std::string& out) {
  if (!valid_entity(entity_id) || end_epoch <= start_epoch || start_epoch < 0 || (types_mask & 0x3F) == 0) return false;
  std::string types;
  for (uint8_t t = 0; t < 6; t++) {
    if (!(types_mask & (1u << t))) continue;
    if (!types.empty()) types += ',';
    types += std::string("\"") + stats_type_name((StatsType)t) + "\"";
  }
  out = "{\"id\":" + std::to_string(id) + ",\"type\":\"recorder/statistics_during_period\",\"start_time\":\"" +
        iso_utc(start_epoch) + "\",\"end_time\":\"" + iso_utc(end_epoch) + "\",\"statistic_ids\":[\"" + entity_id +
        "\"],\"period\":\"" + stats_period_name(period) + "\",\"types\":[" + types + "]}";
  return true;
}
