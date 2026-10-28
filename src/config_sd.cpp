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
    case Status::SecretsTooLarge: return "secrets.yaml too large";
    case Status::SecretsReadError: return "secrets.yaml read error";
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

namespace {
// Reads `path` (SD mounted). NoFile if absent; `tooLarge` / `readError` are the caller's statuses.
Status readOne(const char *path, std::string &out, Status tooLarge, Status readError) {
  out.clear();
  Status status = Status::Ok;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    status = Status::NoFile;
  } else if (f.size() > config::kMaxFileBytes) {
    status = tooLarge;
  } else {
    const size_t size = f.size();
    out.resize(size);
    size_t got = size ? f.read(reinterpret_cast<uint8_t *>(&out[0]), size) : 0;
    if (got != size) {
      out.clear();
      status = readError;
    }
  }
  if (f) f.close();
  return status;
}

// Size + mtime of `path` (SD mounted).
Status statOne(const char *path, uint32_t &size, uint32_t &mtime, Status tooLarge) {
  size = mtime = 0;
  Status status = Status::Ok;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    status = Status::NoFile;
  } else if (f.size() > config::kMaxFileBytes) {
    status = tooLarge;
  } else {
    size = uint32_t(f.size());
    mtime = uint32_t(f.getLastWrite());
  }
  if (f) f.close();
  return status;
}
}  // namespace

Status readFile(std::string &out) {
  if (!mount()) {
    digitalWrite(kPinCs, HIGH);
    out.clear();
    return Status::NoCard;
  }
  Status status = readOne(kPath, out, Status::TooLarge, Status::ReadError);
  unmount();
  return status;
}

Status stat(config::Fingerprint &fp) {
  fp = config::Fingerprint();
  if (!mount()) {
    digitalWrite(kPinCs, HIGH);
    return Status::NoCard;
  }
  Status status = statOne(kPath, fp.size, fp.mtime, Status::TooLarge);
  if (status == Status::Ok) {
    const Status s = statOne(kSecretsPath, fp.secretsSize, fp.secretsMtime, Status::SecretsTooLarge);
    if (s != Status::Ok && s != Status::NoFile) status = s;
  }
  unmount();
  return status;
}

LoadResult load() {
  LoadResult r;
  if (!mount()) {
    digitalWrite(kPinCs, HIGH);
    r.status = Status::NoCard;
    return r;
  }
  std::string text, secrets;
  r.status = readOne(kPath, text, Status::TooLarge, Status::ReadError);
  Status ss = Status::NoFile;
  if (r.status == Status::Ok) {
    ss = readOne(kSecretsPath, secrets, Status::SecretsTooLarge, Status::SecretsReadError);
    if (ss != Status::Ok && ss != Status::NoFile) r.status = ss;
  }
  unmount();
  if (r.status != Status::Ok) return r;
  r.secretsPresent = ss == Status::Ok;
  config::ParseResult pr = config::parseConfig(text);
  if (r.secretsPresent) {
    const config::SecretsResult sr = config::parseSecrets(secrets);
    config::MergeOutcome m = config::mergeSecrets(std::move(pr), &sr);
    r.parse = std::move(m.parse);
  } else {
    config::MergeOutcome m = config::mergeSecrets(std::move(pr), nullptr);
    r.parse = std::move(m.parse);
    r.warnConnectionInConfig = m.warnConnectionInConfig;
  }
  return r;
}

}  // namespace config_sd
