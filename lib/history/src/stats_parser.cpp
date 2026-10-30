#include "stats_parser.h"

#include <cmath>
#include <cstdlib>

#include "iso8601.h"

namespace history {

namespace {
constexpr size_t kMaxStr = 300;
const char* chosenKey(StatsType t) { return stats_type_name(t); }
}  // namespace

StatsParser::StatsParser(const std::string& entity_id, const StatsParams& p) : entity_(entity_id), p_(p) {
  out_.kind = Kind::Numeric;
  out_.start = p.layout.start;
  out_.end = p.layout.end;
  if (p.bars) {
    out_.bucketSeconds = p.layout.bucketSeconds;
    int n = p.layout.bucketCount;
    if (n < 0) n = 0;
    if (n > kMaxBuckets) n = kMaxBuckets;
    out_.buckets.assign((size_t)n, Bucket());
  } else {
    int c = p.layout.columns;
    if (c < 1) c = 1;
    if (c > kMaxColumns) c = kMaxColumns;
    out_.columns.assign((size_t)c, Column());
  }
}

void StatsParser::feed(const char* data, size_t len) {
  for (size_t i = 0; i < len; i++) onChar(data[i]);
}

void StatsParser::onChar(char c) {
  if (bad_) return;
  if (inStr_) {
    if (esc_) {
      esc_ = false;
      if (str_.size() < kMaxStr) str_ += c;
    } else if (c == '\\') {
      esc_ = true;
    } else if (c == '"') {
      inStr_ = false;
      endString();
    } else if (str_.size() < kMaxStr) {
      str_ += c;
    }
    return;
  }
  switch (c) {
    case '"':
      inStr_ = true;
      str_.clear();
      if (!started_) bad_ = true;
      return;
    case '{':
    case '[':
      endToken();
      if (!started_) {
        if (c != '{') { bad_ = true; return; }
        started_ = true;
      }
      if (depth_ >= 5) { bad_ = true; return; }
      depth_++;
      cont_[depth_] = c;
      key_[depth_].clear();
      expectKey_ = c == '{';
      if (c == '[' && depth_ == 3 && key_[2] == entity_ && key_[1] == "result") found_ = true;
      if (c == '{' && depth_ == 2 && key_[1] == "result") sawResult_ = true;
      if (c == '{' && depth_ == 4 && key_[2] == entity_ && key_[1] == "result") {
        hS_ = hE_ = hMean_ = hMin_ = hMax_ = hChosen_ = false;
      }
      return;
    case '}':
    case ']':
      endToken();
      if (depth_ <= 0) { bad_ = true; return; }
      if (c == '}' && depth_ == 4 && found_ && key_[2] == entity_ && key_[1] == "result") bucketDone();
      depth_--;
      expectKey_ = false;
      return;
    case ':':
      expectKey_ = false;
      return;
    case ',':
      endToken();
      expectKey_ = depth_ > 0 && cont_[depth_] == '{';
      return;
    case ' ': case '\t': case '\r': case '\n':
      endToken();
      return;
    default:
      if (!started_) { bad_ = true; return; }
      if (tok_.size() < 40) tok_ += c;
  }
}

void StatsParser::endString() {
  if (depth_ > 0 && cont_[depth_] == '{' && expectKey_) {
    key_[depth_] = str_;
    expectKey_ = false;
    return;
  }
  value(str_, true);
}

void StatsParser::endToken() {
  if (tok_.empty()) return;
  std::string t;
  t.swap(tok_);
  value(t, false);
}

bool StatsParser::timeMs(const std::string& s, bool isString, bool& ok, int64_t& ms) const {
  ok = false;
  if (isString) {
    int64_t sec;
    if (!parseIso8601(s.c_str(), s.size(), sec)) return true;
    ms = sec * 1000;
    ok = true;
    return true;
  }
  if (s == "null") return true;
  char* end = nullptr;
  double v = strtod(s.c_str(), &end);
  if (end == s.c_str() || !std::isfinite(v) || v < 0 || v > 1e15) return true;  // ms; clamp before the int64 cast
  ms = v >= 1e11 ? (int64_t)v : (int64_t)(v * 1000.0);  // tolerate epoch seconds
  ok = true;
  return true;
}

void StatsParser::value(const std::string& s, bool isString) {
  if (depth_ == 4 && key_[2] == entity_ && key_[1] == "result") {
    const std::string& k = key_[4];
    if (k == "start" || k == "end") {
      bool ok;
      int64_t ms = 0;
      timeMs(s, isString, ok, ms);
      if (k == "start") { hS_ = ok; s_ = ms; } else { hE_ = ok; e_ = ms; }
      return;
    }
    if (isString) return;
    char* end = nullptr;
    double v = strtod(s.c_str(), &end);
    bool ok = s != "null" && end != s.c_str() && std::isfinite(v) && std::fabs(v) <= 3e38;  // fits a float
    if (k == "mean") { hMean_ = ok; mean_ = v; }
    else if (k == "min") { hMin_ = ok; min_ = v; }
    else if (k == "max") { hMax_ = ok; max_ = v; }
    if (k == chosenKey(p_.statistic)) { hChosen_ = ok; chosen_ = v; }
  }
}

void StatsParser::bucketDone() {
  buckets_++;
  if (!hS_) return;
  int64_t sec = s_ / 1000;
  int64_t esec = hE_ ? e_ / 1000 : sec + p_.periodSeconds;
  if (esec <= sec) esec = sec + p_.periodSeconds;
  if (p_.bars) {
    if (!hChosen_ || p_.layout.bucketSeconds <= 0) return;
    int64_t bs = p_.layout.bucketSeconds;
    int64_t num = sec - p_.layout.start + bs / 2;
    int64_t idx = num >= 0 ? num / bs : -1;  // rounded to the nearest bucket start
    if (idx < 0 || idx >= (int64_t)out_.buckets.size()) return;
    Bucket& b = out_.buckets[(size_t)idx];
    b.min = b.max = b.mean = b.last = (float)chosen_;
    b.has = true;
    points_++;
    return;
  }
  if (!hMean_) return;
  int64_t start = p_.layout.start, dur = p_.layout.end - start;
  if (dur <= 0 || esec <= start || sec >= p_.layout.end) return;
  int64_t cols = (int64_t)out_.columns.size();
  int64_t c0 = sec <= start ? 0 : (sec - start) * cols / dur;
  int64_t c1 = ((esec - start) * cols + dur - 1) / dur;  // exclusive, ceil
  if (c1 > cols) c1 = cols;
  if (c1 <= c0) c1 = c0 + 1;
  if (c0 >= cols) return;
  for (int64_t c = c0; c < c1; c++) {
    Column& col = out_.columns[(size_t)c];
    col.min = (float)(hMin_ ? min_ : mean_);
    col.max = (float)(hMax_ ? max_ : mean_);
    col.last = (float)mean_;
    col.has = true;
  }
  points_++;
}

StatsOutcome StatsParser::finish() {
  endToken();
  if (bad_ || !started_) return StatsOutcome::Malformed;
  if (depth_ != 0 || inStr_) return StatsOutcome::Truncated;
  if (!sawResult_) return StatsOutcome::Malformed;
  if (!found_ || buckets_ == 0) return StatsOutcome::NoStatistics;
  out_.points = points_;
  return StatsOutcome::Ok;
}

Series StatsParser::series() {
  out_.points = points_;
  return out_;
}

}  // namespace history
