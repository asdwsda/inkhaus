#include "cache.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace history {

namespace {
uint32_t fnv(uint32_t h, const void* d, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(d);
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
  return h;
}
template <class T> uint32_t fnvv(uint32_t h, T v) { return fnv(h, &v, sizeof v); }

struct W {
  std::vector<uint8_t>& o;
  template <class T> void put(T v) {
    for (size_t i = 0; i < sizeof(T); ++i) o.push_back(static_cast<uint8_t>((static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(v)) >> (8 * i)) & 0xFF));
  }
  void f32(float f) { uint32_t u; memcpy(&u, &f, 4); put(u); }
};
struct R {
  const uint8_t* p; size_t n, i = 0; bool bad = false;
  template <class T> T get() {
    if (i + sizeof(T) > n) { bad = true; return 0; }
    uint64_t u = 0;
    for (size_t k = 0; k < sizeof(T); ++k) u |= static_cast<uint64_t>(p[i + k]) << (8 * k);
    i += sizeof(T);
    return static_cast<T>(static_cast<std::make_unsigned_t<T>>(u));
  }
  float f32() { uint32_t u = get<uint32_t>(); float f; memcpy(&f, &u, 4); return f; }
};

int16_t quant(float v, float lo, float hi) {
  const double r = hi > lo ? (v - lo) / (hi - lo) : 0.0;
  return static_cast<int16_t>(std::lround(std::min(1.0, std::max(0.0, r)) * 65534.0) - 32767);
}
float dequant(int16_t q, float lo, float hi) { return lo + (hi - lo) * static_cast<float>((q + 32767) / 65534.0); }
}  // namespace

uint32_t paramsHash(const CacheParams& p) {
  uint32_t h = 2166136261u;
  h = fnv(h, p.entity.data(), p.entity.size());
  h = fnvv<int32_t>(h, p.hours);
  h = fnvv<int32_t>(h, p.type);
  h = fnvv<int32_t>(h, p.statistic);
  h = fnvv<int32_t>(h, p.period);
  h = fnvv<int32_t>(h, p.source);
  h = fnvv<int32_t>(h, p.columns);
  h = fnvv<uint8_t>(h, p.hasMin);
  h = fnvv<uint8_t>(h, p.hasMax);
  h = fnvv<float>(h, p.hasMin ? p.min : 0.f);
  h = fnvv<float>(h, p.hasMax ? p.max : 0.f);
  return h;
}

std::vector<uint8_t> serialize(const Series& s, uint32_t hash, int64_t fetchedAt) {
  std::vector<uint8_t> o;
  o.reserve(64 + s.columns.size() * 7 + s.buckets.size() * 17);
  W w{o};
  o.insert(o.end(), {'H', 'S', 'T', '2'});
  w.put<uint32_t>(hash);
  w.put<int64_t>(fetchedAt);
  w.put<int64_t>(s.start);
  w.put<int64_t>(s.end);
  w.put<uint8_t>(static_cast<uint8_t>(s.kind));
  w.put<uint16_t>(static_cast<uint16_t>(s.columns.size()));
  w.put<uint16_t>(static_cast<uint16_t>(s.buckets.size()));
  w.put<int64_t>(s.bucketSeconds);
  float lo = 0, hi = 0;
  s.range(lo, hi);
  w.f32(lo);
  w.f32(hi);
  for (const auto& c : s.columns) {
    if (!c.has) { for (int k = 0; k < 3; ++k) w.put<int16_t>(-32768); continue; }
    w.put<int16_t>(quant(c.min, lo, hi));
    w.put<int16_t>(quant(c.max, lo, hi));
    w.put<int16_t>(quant(c.last, lo, hi));
  }
  for (size_t i = 0; i < s.columns.size(); i += 8) {  // unavailable bitmap
    uint8_t m = 0;
    for (size_t k = 0; k < 8 && i + k < s.columns.size(); ++k) m |= static_cast<uint8_t>(s.columns[i + k].unavail) << k;
    w.put<uint8_t>(m);
  }
  for (const auto& b : s.buckets) {
    w.f32(b.min); w.f32(b.max); w.f32(b.mean); w.f32(b.last);
    w.put<uint8_t>(b.has);
  }
  w.put<uint32_t>(fnv(2166136261u, o.data(), o.size()));
  return o;
}

CacheStatus deserialize(const uint8_t* d, size_t n, uint32_t expected, Series& out, int64_t& fetchedAt) {
  if (!d || n < 8 + 4 || memcmp(d, "HST2", 4) != 0) return CacheStatus::Corrupt;
  R chk{d + n - 4, 4};
  if (chk.get<uint32_t>() != fnv(2166136261u, d, n - 4)) return CacheStatus::Corrupt;
  R r{d, n - 4, 4};
  const uint32_t hash = r.get<uint32_t>();
  Series s;
  const int64_t fetched = r.get<int64_t>();
  s.start = r.get<int64_t>();
  s.end = r.get<int64_t>();
  const uint8_t kind = r.get<uint8_t>();
  const uint16_t nc = r.get<uint16_t>(), nb = r.get<uint16_t>();
  s.bucketSeconds = r.get<int64_t>();
  const float lo = r.f32(), hi = r.f32();
  if (r.bad || kind > 3 || nc > kMaxColumns || nb > kMaxBuckets) return CacheStatus::Corrupt;
  if (r.n - r.i != static_cast<size_t>(nc) * 6 + (static_cast<size_t>(nc) + 7) / 8 + static_cast<size_t>(nb) * 17) return CacheStatus::Corrupt;
  if (hash != expected) return CacheStatus::WrongParams;
  s.kind = static_cast<Kind>(kind);
  s.columns.resize(nc);
  for (auto& c : s.columns) {
    const int16_t a = r.get<int16_t>(), b = r.get<int16_t>(), l = r.get<int16_t>();
    if (a == -32768) continue;
    c = Column{dequant(a, lo, hi), dequant(b, lo, hi), dequant(l, lo, hi), true};
    ++s.points;
  }
  for (size_t i = 0; i < s.columns.size(); i += 8) {
    const uint8_t m = r.get<uint8_t>();
    for (size_t k = 0; k < 8 && i + k < s.columns.size(); ++k)
      if (m >> k & 1 && !s.columns[i + k].has) s.columns[i + k].unavail = true;
  }
  s.buckets.resize(nb);
  for (auto& b : s.buckets) {
    b.min = r.f32(); b.max = r.f32(); b.mean = r.f32(); b.last = r.f32();
    b.has = r.get<uint8_t>() != 0;
  }
  if (r.bad) return CacheStatus::Corrupt;
  out = std::move(s);
  fetchedAt = fetched;
  return CacheStatus::Ok;
}

}  // namespace history
