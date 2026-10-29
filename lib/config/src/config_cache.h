#pragma once
// Binary serialisation of config::Config for the flash cache (task 025). Pure C++17.
// Layout: "MPCC", u8 format, payload, u32 CRC32 of everything before it. All little endian.
// The blob holds the same secrets as config.yaml: never log it.

#include <cstdint>
#include <string>

#include "config_model.h"

namespace config {

// Bump when Config changes, so old caches are rejected.
constexpr uint8_t kCacheFormat = 5;

std::string serializeConfig(const Config &cfg);
// False for any corrupted, truncated, oversized or wrong-format blob; `out` is then unspecified.
bool deserializeConfig(const uint8_t *data, size_t len, Config &out);
inline bool deserializeConfig(const std::string &s, Config &out) {
  return deserializeConfig(reinterpret_cast<const uint8_t *>(s.data()), s.size(), out);
}

// Identifies one version of /config.yaml and /secrets.yaml on SD: size + modification time of each
// (secrets fields are 0 when secrets.yaml does not exist).
struct Fingerprint {
  uint32_t size = 0;
  uint32_t mtime = 0;
  uint32_t secretsSize = 0;
  uint32_t secretsMtime = 0;
  bool operator==(const Fingerprint &o) const {
    return size == o.size && mtime == o.mtime && secretsSize == o.secretsSize &&
           secretsMtime == o.secretsMtime;
  }
};

// Cache file content = fingerprint (16 bytes) + config blob; wrapped so a fingerprint is bound to it.
std::string packCache(const Fingerprint &fp, const Config &cfg);
// Rejects corruption; on success `fp` is the stored fingerprint.
bool unpackCache(const std::string &file, Fingerprint &fp, Config &out);

}  // namespace config
