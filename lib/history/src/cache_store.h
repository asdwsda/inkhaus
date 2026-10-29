#pragma once
// Per-entity history cache (task 053): a small PSRAM map of serialised series (cache.h format, ~4 KB each)
// in front of an optional persistent backend (the SD card on the device). Freshness rules of
// docs/ui/history.md section 6: a cache is fresh for max(10 min, hours x 1 %), older = stale (shown,
// then refreshed). Pure logic; the backend is injected.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "cache.h"

namespace history {

enum class Freshness { Missing, Fresh, Stale };

constexpr int64_t kMinFreshSeconds = 600;
constexpr size_t kMaxCachedEntities = 8;

// Seconds a cache stays fresh for a window of `hours`: max(10 min, hours x 1 %).
int64_t freshSeconds(int hours);
// now <= 0 (no clock) or a fetch time in the future counts as Stale (cannot prove it is fresh).
Freshness classify(int64_t fetchedAt, int64_t now, int hours);

class CacheBackend {
 public:
  virtual ~CacheBackend() = default;
  virtual bool load(const std::string& entity, std::vector<uint8_t>& out) = 0;
  virtual void store(const std::string& entity, const std::vector<uint8_t>& blob) = 0;
};

class CacheStore {
 public:
  explicit CacheStore(CacheBackend* backend = nullptr) : backend_(backend) {}
  void setBackend(CacheBackend* b) { backend_ = b; }

  // Looks the entity up (memory first, then the backend). A hash mismatch or corrupt blob is Missing.
  Freshness lookup(const std::string& entity, uint32_t hash, int64_t now, int hours, Series& out, int64_t& fetchedAt);
  // Stores (memory + backend). The oldest memory entry is dropped beyond kMaxCachedEntities.
  void put(const std::string& entity, uint32_t hash, const Series& s, int64_t fetchedAt);
  size_t memoryEntries() const { return mem_.size(); }

 private:
  struct Entry { std::vector<uint8_t> blob; int64_t fetchedAt = 0; uint32_t seq = 0; };
  std::map<std::string, Entry> mem_;
  CacheBackend* backend_ = nullptr;
  uint32_t seq_ = 0;
};

}  // namespace history
