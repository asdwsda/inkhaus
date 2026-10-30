#include "collector.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace history {

namespace {
// -1 = not binary, 0 = off, 1 = on
int binaryValue(const char* s, size_t n) {
  struct E { const char* w; int v; };
  static const E t[] = {{"on", 1},   {"off", 0},      {"open", 1},    {"closed", 0},
                        {"home", 1}, {"not_home", 0}, {"true", 1},    {"false", 0},
                        {"detected", 1}, {"clear", 0}};
  for (const auto& e : t)
    if (strlen(e.w) == n && !strncmp(e.w, s, n)) return e.v;
  return -1;
}
bool isSkipped(const char* s, size_t n) {
  return n == 0 || (n == 7 && !strncmp(s, "unknown", 7)) || (n == 11 && !strncmp(s, "unavailable", 11));
}
}  // namespace

bool Series::range(float& lo, float& hi) const {
  bool any = false;
  for (const auto& c : columns) {
    if (!c.has) continue;
    if (!any) { lo = c.min; hi = c.max; any = true; }
    lo = std::min(lo, c.min);
    hi = std::max(hi, c.max);
  }
  return any;
}

bool Series::change(size_t i, float& out) const {
  if (i >= buckets.size() || !buckets[i].has) return false;
  if (i == 0) { out = 0; return true; }
  if (!buckets[i - 1].has) return false;
  out = buckets[i].last - buckets[i - 1].last;
  return true;
}

Collector::Collector(const Params& p) : p_(p) {
  if (p_.end <= p_.start) p_.end = p_.start + 1;
  p_.columns = std::max(1, std::min(p_.columns, kMaxColumns));
  p_.bucketCount = std::max(0, std::min(p_.bucketCount, kMaxBuckets));
  if (p_.bucketSeconds <= 0) p_.bucketCount = 0;
  cols_.assign(static_cast<size_t>(p_.columns), Column{});
  bk_.assign(static_cast<size_t>(p_.bucketCount), Bucket{});
  bkW_.assign(bk_.size(), 0.0);
  bkT_.assign(bk_.size(), 0.0);
}

int Collector::column(int64_t t) const {
  if (t < p_.start) return 0;
  int64_t c = (t - p_.start) * p_.columns / (p_.end - p_.start);
  return static_cast<int>(std::min<int64_t>(c, p_.columns - 1));
}

void Collector::onPoint(int64_t t, const char* s, size_t n) {
  if (kind_ == Kind::Unsupported) return;
  if (isSkipped(s, n)) {
    if (!inGap_ && t < p_.end) { inGap_ = true; gapT_ = t; }
    return;
  }
  double v;
  Kind k;
  int b = binaryValue(s, n);
  if (b >= 0) {
    v = b;
    k = Kind::Binary;
  } else {
    char buf[40];
    if (n >= sizeof buf) { if (kind_ == Kind::Unknown) kind_ = Kind::Unsupported; return; }
    memcpy(buf, s, n);
    buf[n] = 0;
    char* e = nullptr;
    v = strtod(buf, &e);
    if (e == buf || *e != 0 || !std::isfinite(v)) {
      if (kind_ == Kind::Unknown) kind_ = Kind::Unsupported;
      return;
    }
    k = Kind::Numeric;
  }
  if (kind_ == Kind::Unknown) kind_ = k;
  if (k != kind_) return;
  if (t >= p_.end) return;
  ++points_;
  const int gf = inGap_ ? gapFirst() : p_.columns;
  inGap_ = false;
  value(t, v);
  const int c = column(t);
  for (int i = gf; i < c; ++i) cols_[i] = Column{0, 0, 0, false, true};
}

// First column a gap in effect covers: the one after the last column with a value.
int Collector::gapFirst() const {
  if (!inGap_) return 0;
  const int gc = column(gapT_);
  return hasCur_ ? std::max(gc, curCol_ + 1) : gc;
}

void Collector::value(int64_t t, double v) {
  const int c = column(t);
  auto upd = [&](Column& col, double x) {
    col.min = std::min<float>(col.min, static_cast<float>(x));
    col.max = std::max<float>(col.max, static_cast<float>(x));
    col.last = static_cast<float>(x);
  };
  if (!hasCur_) {
    cols_[c] = Column{static_cast<float>(v), static_cast<float>(v), static_cast<float>(v), true};
  } else if (c > curCol_) {
    const float cv = static_cast<float>(cur_);
    for (int i = curCol_ + 1; i <= c; ++i) cols_[i] = Column{cv, cv, cv, true};
    upd(cols_[c], v);
  } else {
    upd(cols_[c], v);
  }
  if (p_.bucketCount) {
    const int64_t tt = std::max(t, p_.start);
    if (!hasCur_) lastT_ = tt;
    advanceBuckets(tt);
    if (tt < p_.start + p_.bucketSeconds * p_.bucketCount)
      bucketTouch(static_cast<int>((tt - p_.start) / p_.bucketSeconds), v);
  }
  curCol_ = hasCur_ ? std::max(curCol_, c) : c;
  hasCur_ = true;
  cur_ = v;
}

void Collector::bucketTouch(int b, double v) {
  Bucket& k = bk_[b];
  if (!k.has) { k.min = k.max = static_cast<float>(v); k.has = true; }
  k.min = std::min<float>(k.min, static_cast<float>(v));
  k.max = std::max<float>(k.max, static_cast<float>(v));
  k.last = static_cast<float>(v);
}

// Accrues the current value over [lastT_, t) into the buckets it spans.
void Collector::advanceBuckets(int64_t t) {
  if (!hasCur_ || !p_.bucketCount) { lastT_ = t; return; }
  const int64_t bEndAll = p_.start + p_.bucketSeconds * p_.bucketCount;
  t = std::min(t, bEndAll);
  while (lastT_ < t) {
    int b = static_cast<int>((lastT_ - p_.start) / p_.bucketSeconds);
    int64_t segEnd = std::min(t, p_.start + p_.bucketSeconds * (b + 1));
    bucketTouch(b, cur_);
    bkW_[b] += cur_ * static_cast<double>(segEnd - lastT_);
    bkT_[b] += static_cast<double>(segEnd - lastT_);
    lastT_ = segEnd;
  }
}

Series Collector::finish() {
  if (!done_) {
    done_ = true;
    if (hasCur_) {
      const float cv = static_cast<float>(cur_);
      const int fillEnd = inGap_ ? std::min(p_.columns, gapFirst()) : p_.columns;
      for (int i = curCol_ + 1; i < fillEnd; ++i) cols_[i] = Column{cv, cv, cv, true};
      if (p_.bucketCount) advanceBuckets(std::min(p_.end, p_.start + p_.bucketSeconds * p_.bucketCount));
    }
    if (inGap_)
      for (int i = gapFirst(); i < p_.columns; ++i) cols_[i] = Column{0, 0, 0, false, true};
    for (size_t i = 0; i < bk_.size(); ++i)
      if (bkT_[i] > 0) bk_[i].mean = static_cast<float>(bkW_[i] / bkT_[i]);
      else if (bk_[i].has) bk_[i].mean = bk_[i].last;
  }
  Series s;
  s.kind = kind_;
  s.start = p_.start;
  s.end = p_.end;
  s.columns = cols_;
  s.bucketSeconds = p_.bucketSeconds;
  s.buckets = bk_;
  s.points = points_;
  return s;
}

}  // namespace history
