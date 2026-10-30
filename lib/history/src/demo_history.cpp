#include "demo_history.h"

#include <algorithm>
#include <cmath>

namespace history {
namespace {

uint32_t mix(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
uint32_t seedOf(const std::string& s) {
  uint32_t h = 2166136261U;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619U; }
  return h;
}
double unit(uint32_t seed, int64_t k) { return mix(seed ^ mix((uint32_t)k)) / 4294967295.0; }  // 0..1

bool has(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }

enum class Prof { Temp, Humidity, Power, Battery, Energy, Generic };
struct Profile { Prof p; float lo, hi, base, amp, noise; };

Profile profileOf(const std::string& id) {
  if (has(id, "humid")) return {Prof::Humidity, 20, 90, 52, 9, 2.5f};
  if (has(id, "temp")) return {Prof::Temp, 8, 35, 21, 2.2f, 0.4f};
  if (has(id, "energy") || has(id, "kwh")) return {Prof::Energy, 0, 1e6f, 0, 0, 0};
  if (has(id, "power") || has(id, "watt") || has(id, "consumption")) return {Prof::Power, 0, 2500, 0, 0, 0};
  if (has(id, "battery")) return {Prof::Battery, 5, 100, 0, 0, 0};
  return {Prof::Generic, 0, 100, 50, 18, 4};
}

// Smooth noise in [-1, 1]: hashed knots every 20 minutes, cosine-interpolated.
double smooth(uint32_t seed, int64_t t) {
  const int64_t k = t / 1200;
  const double f = (double)(t - k * 1200) / 1200.0, w = (1 - std::cos(f * 3.14159265358979)) / 2;
  const double a = unit(seed, k) * 2 - 1, b = unit(seed, k + 1) * 2 - 1;
  return a + (b - a) * w;
}

double valueAt(const Profile& pr, uint32_t seed, int64_t t, int64_t end) {
  const double dayPhase = (double)(t % 86400) / 86400.0 + unit(seed, -1) * 0.1;
  double v = 0;
  switch (pr.p) {
    case Prof::Power: {  // appliance cycles: off / standby / running, 45 min slots
      const double r = unit(seed, t / 2700);
      v = r < 0.45 ? 2 + 4 * smooth(seed, t) : r < 0.75 ? 120 + 40 * smooth(seed, t) : 900 + 800 * unit(seed + 1, t / 2700);
      break;
    }
    case Prof::Battery: {  // slow discharge ending at a plausible level
      const double hoursLeft = (double)(end - t) / 3600.0;
      v = 62 + unit(seed, -2) * 25 + hoursLeft * 0.35 + smooth(seed, t) * 0.6;
      break;
    }
    default:
      v = pr.base + pr.amp * std::sin((dayPhase - 0.3) * 2 * 3.14159265358979) + pr.noise * smooth(seed, t);
  }
  return std::min<double>(pr.hi, std::max<double>(pr.lo, v));
}

double binaryAt(uint32_t seed, int64_t t) { return unit(seed, t / 1800) < 0.3 ? 1.0 : 0.0; }

}  // namespace

void demoRange(const std::string& entity, float& lo, float& hi) {
  const Profile pr = profileOf(entity);
  lo = pr.lo; hi = pr.hi;
}

Series demoSeries(const DemoParams& p) {
  Series s;
  s.start = p.start; s.end = p.end;
  s.kind = p.binary ? Kind::Binary : Kind::Numeric;
  if (p.end <= p.start) return s;
  const uint32_t seed = seedOf(p.entity);
  const Profile pr = profileOf(p.entity);

  if (p.bar && p.bucketCount > 0 && p.bucketSeconds > 0) {
    const int n = std::min(p.bucketCount, kMaxBuckets);
    s.bucketSeconds = p.bucketSeconds;
    s.buckets.assign((size_t)n, Bucket());
    double cum = 1000 + unit(seed, -3) * 3000;  // a running total (energy-like), grows each bucket
    for (int i = 0; i < n; ++i) {
      const int64_t t = p.start + (int64_t)i * p.bucketSeconds;
      const double hour = (double)(t % 86400) / 3600.0;
      const double load = 0.15 + 0.5 * std::max(0.0, std::sin((hour - 6) / 16 * 3.14159265358979)) + 0.1 * unit(seed, t);
      cum += load * (double)p.bucketSeconds / 3600.0;
      Bucket& b = s.buckets[(size_t)i];
      b.min = b.max = b.mean = b.last = (float)cum;
      b.has = t < p.end;  // the bucket in progress at the end still counts; later ones do not
      if (b.has) ++s.points;
    }
    return s;
  }

  const int n = std::max(1, std::min(p.columns, kMaxColumns));
  s.columns.assign((size_t)n, Column());
  const double span = (double)(p.end - p.start) / n;
  for (int c = 0; c < n; ++c) {
    Column& col = s.columns[(size_t)c];
    double lo = 1e30, hi = -1e30, last = 0;
    for (int k = 0; k < 4; ++k) {  // sub-samples so a step inside a column shows as min != max
      const int64_t t = p.start + (int64_t)(span * (c + (k + 0.5) / 4.0));
      last = p.binary ? binaryAt(seed, t) : valueAt(pr, seed, t, p.end);
      lo = std::min(lo, last); hi = std::max(hi, last);
    }
    col.min = (float)lo; col.max = (float)hi; col.last = (float)last; col.has = true;
    ++s.points;
  }
  return s;
}

}  // namespace history
