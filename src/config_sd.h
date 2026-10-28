#pragma once
// SD card access for config.yaml and secrets.yaml. Hardware wrapper: all parsing is in lib/config.

#include <string>

#include "config_cache.h"
#include "config_model.h"

namespace config_sd {

enum class Status {
  Ok,          // file read (it may still have validation errors, see parse)
  NoCard,      // SD card missing or not mountable
  NoFile,      // /config.yaml not found
  TooLarge,    // file larger than config::kMaxFileBytes (not read)
  ReadError,   // short read or I/O error
  SecretsTooLarge,   // /secrets.yaml larger than config::kMaxFileBytes
  SecretsReadError,  // /secrets.yaml short read or I/O error
};

const char *statusText(Status s);

constexpr const char *kPath = "/config.yaml";
constexpr const char *kSecretsPath = "/secrets.yaml";

// Mounts / unmounts the card on the shared SPI bus (for other SD users, e.g. provision). Every
// successful mount() must be paired with unmount(). mount() waits for the display first.
bool mount();
void unmount();

// Reads /config.yaml from the SD root into `out`. Mounts and unmounts the card
// around the read so the SPI bus is left to the display afterwards.
Status readFile(std::string &out);

struct LoadResult {
  Status status = Status::NoCard;
  // Only meaningful when status == Ok: config.yaml parsed and merged with secrets.yaml (if any).
  config::ParseResult parse;
  bool secretsPresent = false;
  bool warnConnectionInConfig = false;  // no secrets.yaml, connection still in config.yaml
};

// Size + mtime of /config.yaml and /secrets.yaml without reading them (mounts and unmounts like
// readFile). A missing secrets.yaml is not an error: its fingerprint fields stay 0.
Status stat(config::Fingerprint &fp);

// Reads both files in one mount, parses and merges them (config::mergeSecrets).
LoadResult load();

}  // namespace config_sd
