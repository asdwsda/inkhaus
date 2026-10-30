#pragma once
// recorder/statistics_during_period command (task 054, design 046 part B).
#include <cstdint>
#include <string>

#include "stats_types.h"

// {"id":<id>,"type":"recorder/statistics_during_period","start_time":..,"end_time":..,
//  "statistic_ids":["<entity>"],"period":"hour","types":["mean","min","max"]}
// types_mask: OR of stats_bit(). Returns false (out untouched) for an odd entity id, an empty
// mask or end <= start.
bool build_statistics_command(int id, const std::string& entity_id, int64_t start_epoch, int64_t end_epoch,
                              StatsPeriod period, uint8_t types_mask, std::string& out);

// JSON string literal with quotes (escapes ", \ and control characters).
std::string json_quote(const std::string& s);
