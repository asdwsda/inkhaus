#include "cache_store.h"

namespace history {

int64_t freshSeconds(int hours) {
  const int64_t pct = static_cast<int64_t>(hours) * 3600 / 100;
  return pct > kMinFreshSeconds ? pct : kMinFreshSeconds;
}

Freshness classify(int64_t fetchedAt, int64_t now, int hours) {
  if (now <= 0 || fetchedAt <= 0 || fetchedAt > now) return Freshness::Stale;
  return now - fetchedAt <= freshSeconds(hours) ? Freshness::Fresh : Freshness::Stale;
}

Freshness CacheStore::lookup(const std::string& entity, uint32_t hash, int64_t now, int hours, Series& out,
                             int64_t& fetchedAt) {
  auto it = mem_.find(entity);
  if (it == mem_.end() && backend_) {
    Entry e;
    if (backend_->load(entity, e.blob)) {
      Series tmp;
      int64_t at = 0;
      if (deserialize(e.blob.data(), e.blob.size(), hash, tmp, at) == CacheStatus::Ok) {
        e.fetchedAt = at;
        e.seq = ++seq_;
        it = mem_.emplace(entity, std::move(e)).first;
      }
    }
  }
  if (it == mem_.end()) return Freshness::Missing;
  Series s;
  int64_t at = 0;
  if (deserialize(it->second.blob.data(), it->second.blob.size(), hash, s, at) != CacheStatus::Ok) {
    mem_.erase(it);  // other parameters (config edited) or corrupt: refetch
    return Freshness::Missing;
  }
  it->second.seq = ++seq_;
  out = std::move(s);
  fetchedAt = at;
  return classify(at, now, hours);
}

void CacheStore::put(const std::string& entity, uint32_t hash, const Series& s, int64_t fetchedAt) {
  Entry e;
  e.blob = serialize(s, hash, fetchedAt);
  e.fetchedAt = fetchedAt;
  e.seq = ++seq_;
  if (backend_) backend_->store(entity, e.blob);
  mem_[entity] = std::move(e);
  while (mem_.size() > kMaxCachedEntities) {
    auto oldest = mem_.begin();
    for (auto i = mem_.begin(); i != mem_.end(); ++i)
      if (i->second.seq < oldest->second.seq) oldest = i;
    mem_.erase(oldest);
  }
}

}  // namespace history
