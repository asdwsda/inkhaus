#pragma once
// SD card backend of the history cache (task 053): one small file per entity, /hist/<hash>.bin.
// Hardware wrapper; the cache logic and format are in lib/history. Mounts and unmounts the card around
// each access (shared SPI bus with the display).
#include "cache_store.h"

namespace history_cache_sd {

class Backend : public history::CacheBackend {
 public:
  bool load(const std::string &entity, std::vector<uint8_t> &out) override;
  void store(const std::string &entity, const std::vector<uint8_t> &blob) override;
};

}  // namespace history_cache_sd
