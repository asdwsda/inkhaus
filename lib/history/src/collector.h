#pragma once
// Downsampler + bucket aggregator behind the Scanner (a PointSink).
//
// Downsampling: the time range [start, end) is split into `columns` pixel columns. Per column
// min / max / last of the value in effect during the column are kept (HA history is a step
// function: a value holds until the next entry). Columns with no entry carry the previous value
// forward; columns before the first entry have has=false. Entries before `start` land in
// column 0 (the value at start); entries at/after `end` are ignored.
//
// Kind: the first parseable state decides. Numeric ("21.4") -> Numeric. on/off, open/closed,
// home/not_home, true/false, detected/clear -> Binary (values 0/1; min != max in a column =
// "changed within it"). unavailable/unknown/"" are skipped. Any other first string -> Unsupported
// (no data is collected). Later entries of the other kind are skipped.
//
// Buckets (bars): optional, `bucketCount` buckets of `bucketSeconds` from `start`; per bucket
// min / max / time-weighted mean / last. change = last - previous bucket's last (see change()).
//
// RAM: columns <= kMaxColumns (13 B each, ~13 KB), buckets <= kMaxBuckets (24 B each). Nothing
// else grows with the body size.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "scanner.h"

namespace history {

constexpr int kMaxColumns = 1024;
constexpr int kMaxBuckets = 128;

enum class Kind : uint8_t { Unknown = 0, Numeric = 1, Binary = 2, Unsupported = 3 };

struct Column {
  float min = 0, max = 0, last = 0;
  bool has = false;
};

struct Bucket {
  float min = 0, max = 0, mean = 0, last = 0;
  bool has = false;
};

struct Params {
  int64_t start = 0, end = 0;  // epoch seconds, end > start
  int columns = 0;             // 1..kMaxColumns
  int64_t bucketSeconds = 0;   // 0 = no buckets
  int bucketCount = 0;         // 0..kMaxBuckets
};

struct Series {
  Kind kind = Kind::Unknown;
  int64_t start = 0, end = 0;
  std::vector<Column> columns;
  int64_t bucketSeconds = 0;
  std::vector<Bucket> buckets;
  size_t points = 0;  // accepted entries
  bool empty() const { return points == 0; }
  // Value range over all columns (min of mins, max of maxes). False if no data.
  bool range(float& lo, float& hi) const;
  // Bar value "change" for bucket i: last[i] - last[i-1] (bucket 0: last - first value seen = 0
  // unless an earlier bucket exists). False if bucket i or its predecessor (i > 0) has no data.
  bool change(size_t i, float& out) const;
};

class Collector : public PointSink {
 public:
  explicit Collector(const Params& p);
  void onPoint(int64_t epoch, const char* state, size_t len) override;
  // Call once after Scanner::finish(). Fills trailing columns / buckets and returns the result.
  Series finish();

  Kind kind() const { return kind_; }

 private:
  void value(int64_t t, double v);
  int column(int64_t t) const;
  void advanceBuckets(int64_t t);
  void bucketTouch(int b, double v);

  Params p_;
  Kind kind_ = Kind::Unknown;
  std::vector<Column> cols_;
  std::vector<Bucket> bk_;
  std::vector<double> bkW_, bkT_;  // weighted sum, covered seconds
  bool hasCur_ = false;
  double cur_ = 0;
  int curCol_ = 0;
  int64_t lastT_ = 0;
  size_t points_ = 0;
  bool done_ = false;
};

}  // namespace history
