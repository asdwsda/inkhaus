#pragma once
// Streaming parser for the HA `recorder/statistics_during_period` result message (task 054):
//   {"id":1,"type":"result","success":true,"result":{"sensor.x":[
//      {"start":1761696000000,"end":1761699600000,"mean":21.4,"min":21.0,"max":21.9,"change":0.3}, ...]}}
// `start`/`end` are epoch milliseconds (HA sends ms) or ISO-8601 strings; missing / null values
// make that bucket "no data". Fixed memory: no DOM, one small string buffer per token.
//
// Line mode: every stat bucket sets the pixel columns its span covers: min = stat min, max =
// stat max (both fall back to the mean when absent), last = mean. Columns outside every bucket
// stay has=false (no carry-forward: statistics have real gaps).
// Bar mode: one Bucket per stat bucket (index = round((start - Params.start) / bucketSeconds)),
// min = max = mean = last = the chosen statistic; Series::change then works on `last`.
#include <cstddef>
#include <cstdint>
#include <string>

#include "collector.h"
#include "stats_types.h"

namespace history {

enum class StatsOutcome : uint8_t {
  Ok,
  NoStatistics,  // valid message, but the entity has no (or an empty) statistics list
  Truncated,     // input ended inside the JSON
  Malformed,     // not the expected shape
};

struct StatsParams {
  Params layout;                       // start / end / columns, and bucketSeconds / bucketCount for bars
  bool bars = false;
  StatsType statistic = StatsType::Mean;  // bars: which statistic goes into the bucket
  int64_t periodSeconds = 3600;        // span of a bucket without `end`
};

class StatsParser {
 public:
  StatsParser(const std::string& entity_id, const StatsParams& p);
  void feed(const char* data, size_t len);
  // Call once at the end. On Ok the series is available via series().
  StatsOutcome finish();
  Series series();
  size_t buckets() const { return buckets_; }  // stat buckets seen for the entity

 private:
  void onChar(char c);
  void endString();
  void endToken();
  void value(const std::string& s, bool isString);
  void bucketDone();
  void put(int64_t s, int64_t e, bool hasMean, double mean, bool hasMin, double mn, bool hasMax, double mx,
           bool hasChosen, double chosen);
  bool timeMs(const std::string& s, bool isString, bool& ok, int64_t& ms) const;

  std::string entity_;
  StatsParams p_;
  std::string str_, tok_;
  std::string key_[6];
  char cont_[6] = {0};  // container type per depth ('{' or '[')
  int depth_ = 0;
  bool inStr_ = false, esc_ = false, expectKey_ = false, bad_ = false, started_ = false;
  bool sawResult_ = false, found_ = false;
  size_t buckets_ = 0;
  // Current bucket fields.
  bool hS_, hE_;
  int64_t s_, e_;
  bool hMean_, hMin_, hMax_, hChosen_;
  double mean_, min_, max_, chosen_;

  Series out_;
  size_t points_ = 0;
};

}  // namespace history
