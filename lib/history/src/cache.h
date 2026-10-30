#pragma once
// Cache (de)serialisation of a downsampled Series, plus the params hash that invalidates it.
//
// Layout (little endian), ~6 B per column: "HST2", u32 paramsHash, i64 fetchedAt, i64 start,
// i64 end, u8 kind, u16 columns, u16 buckets, i64 bucketSeconds, f32 lo, f32 hi, then per column
// i16 min, i16 max, i16 last (value scaled into [lo,hi] over 0..65534, -32768 = no data), then
// per bucket 4 x f32 + u8 has, then a u32 FNV-1a checksum over everything before it.
// 700 columns = ~4.3 KB. Quantisation error <= (hi-lo)/65534.
#include <cstdint>
#include <string>
#include <vector>

#include "collector.h"

namespace history {

struct CacheParams {
  std::string entity;
  int hours = 24;
  int type = 0, statistic = 0, period = 0, source = 0;  // enum values of lib/config, any stable ints
  int columns = 0;
  bool hasMin = false, hasMax = false;
  float min = 0, max = 0;
};

// FNV-1a 32 over all fields. A config edit that changes any of them changes the hash.
uint32_t paramsHash(const CacheParams& p);

std::vector<uint8_t> serialize(const Series& s, uint32_t hash, int64_t fetchedAt);

enum class CacheStatus { Ok, Corrupt, WrongParams };
// Fails with Corrupt on bad magic / size / checksum, WrongParams if the stored hash differs from
// `expectedHash`. `out` and `fetchedAt` are only written on Ok.
CacheStatus deserialize(const uint8_t* data, size_t len, uint32_t expectedHash, Series& out, int64_t& fetchedAt);

}  // namespace history
