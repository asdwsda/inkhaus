#include "config_sd.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

namespace config_sd {
namespace {
// M5Paper: the SD card shares the SPI bus with the IT8951 display controller
// (CS 15); only its chip select differs.
constexpr int kPinSck = 14;
constexpr int kPinMiso = 13;
constexpr int kPinMosi = 12;
constexpr int kPinCs = 4;
constexpr uint32_t kSdHz = 20000000;
}  // namespace

const char *statusText(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::NoCard: return "no SD card";
    case Status::NoFile: return "config.yaml not found";
    case Status::TooLarge: return "config.yaml too large";
    case Status::ReadError: return "config.yaml read error";
  }
  return "?";
}

bool mount() {
  // Never touch the shared SPI bus while an EPD transfer is running.
  M5.Display.waitDisplay();

  // Keep the display controller deselected while the card is accessed.
  pinMode(kPinCs, OUTPUT);
  digitalWrite(kPinCs, HIGH);
  SPI.begin(kPinSck, kPinMiso, kPinMosi, kPinCs);
  return SD.begin(kPinCs, SPI, kSdHz);
}

void unmount() {
  SD.end();
  digitalWrite(kPinCs, HIGH);
}

Status readFile(std::string &out) {
  out.clear();
  Status status = Status::Ok;
  if (!mount()) {
    digitalWrite(kPinCs, HIGH);
    return Status::NoCard;
  }
  {
    File f = SD.open(kPath, FILE_READ);
    if (!f || f.isDirectory()) {
      status = Status::NoFile;
    } else if (f.size() > config::kMaxFileBytes) {
      status = Status::TooLarge;
    } else {
      const size_t size = f.size();
      out.resize(size);
      size_t got = size ? f.read(reinterpret_cast<uint8_t *>(&out[0]), size) : 0;
      if (got != size) {
        out.clear();
        status = Status::ReadError;
      }
    }
    if (f) f.close();
  }
  unmount();
  return status;
}

Status stat(config::Fingerprint &fp) {
  fp = config::Fingerprint();
  if (!mount()) {
    digitalWrite(kPinCs, HIGH);
    return Status::NoCard;
  }
  Status status = Status::Ok;
  {
    File f = SD.open(kPath, FILE_READ);
    if (!f || f.isDirectory()) {
      status = Status::NoFile;
    } else if (f.size() > config::kMaxFileBytes) {
      status = Status::TooLarge;
    } else {
      fp.size = uint32_t(f.size());
      fp.mtime = uint32_t(f.getLastWrite());
    }
    if (f) f.close();
  }
  unmount();
  return status;
}

LoadResult load() {
  LoadResult r;
  std::string text;
  r.status = readFile(text);
  if (r.status == Status::Ok) r.parse = config::parseConfig(text);
  return r;
}

}  // namespace config_sd
