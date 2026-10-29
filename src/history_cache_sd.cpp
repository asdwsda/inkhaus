#include "history_cache_sd.h"

#include <Arduino.h>
#include <SD.h>

#include "config_sd.h"

namespace history_cache_sd {

namespace {
constexpr size_t kMaxFile = 16 * 1024;  // 1024 columns x 6 B + buckets stays far below

std::string pathFor(const std::string &entity) {
  uint32_t h = 2166136261u;
  for (char ch : entity) h = (h ^ uint8_t(ch)) * 16777619u;
  char b[24];
  snprintf(b, sizeof b, "/hist/%08x.bin", (unsigned)h);
  return b;
}
}  // namespace

// UNVERIFIED: the SD mount (~tens of ms) between taps is acceptable, and a card without /hist is created on
// the first store. A missing card is a silent miss.
bool Backend::load(const std::string &entity, std::vector<uint8_t> &out) {
  if (!config_sd::mount()) return false;
  bool ok = false;
  File f = SD.open(pathFor(entity).c_str(), FILE_READ);
  if (f) {
    const size_t n = f.size();
    if (n > 0 && n <= kMaxFile) {
      out.assign(n, 0);
      ok = f.read(out.data(), n) == n;
    }
    f.close();
  }
  config_sd::unmount();
  return ok;
}

void Backend::store(const std::string &entity, const std::vector<uint8_t> &blob) {
  if (blob.empty() || blob.size() > kMaxFile || !config_sd::mount()) return;
  SD.mkdir("/hist");
  const std::string p = pathFor(entity);
  SD.remove(p.c_str());
  File f = SD.open(p.c_str(), FILE_WRITE);
  if (f) {
    f.write(blob.data(), blob.size());
    f.close();
  }
  config_sd::unmount();
}

}  // namespace history_cache_sd
