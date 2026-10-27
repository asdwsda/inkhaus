#pragma once
// SD card access for config.yaml. Hardware wrapper: all parsing is in lib/config.

#include <string>

#include "config_model.h"

namespace config_sd {

enum class Status {
  Ok,          // file read (it may still have validation errors, see parse)
  NoCard,      // SD card missing or not mountable
  NoFile,      // /config.yaml not found
  TooLarge,    // file larger than config::kMaxFileBytes (not read)
  ReadError,   // short read or I/O error
};

const char *statusText(Status s);

constexpr const char *kPath = "/config.yaml";

// Reads /config.yaml from the SD root into `out`. Mounts and unmounts the card
// around the read so the SPI bus is left to the display afterwards.
Status readFile(std::string &out);

struct LoadResult {
  Status status = Status::NoCard;
  config::ParseResult parse;  // only meaningful when status == Ok
};

// readFile() + config::parseConfig().
LoadResult load();

}  // namespace config_sd
