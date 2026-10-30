#pragma once
// Statistics period / type enums shared by the HA command builder (lib/ha_client) and the
// statistics parser (task 054). Kept here so lib/history has no dependency on lib/ha_client.
#include <cstdint>

enum class StatsPeriod : uint8_t { FiveMinute, Hour, Day };
enum class StatsType : uint8_t { Mean, Min, Max, Sum, Change, State };

constexpr uint8_t stats_bit(StatsType t) { return (uint8_t)(1u << (uint8_t)t); }
constexpr uint8_t kStatsLineTypes = stats_bit(StatsType::Mean) | stats_bit(StatsType::Min) | stats_bit(StatsType::Max);

const char* stats_period_name(StatsPeriod p);  // "5minute" | "hour" | "day"
const char* stats_type_name(StatsType t);      // "mean" ...
int64_t stats_period_seconds(StatsPeriod p);
