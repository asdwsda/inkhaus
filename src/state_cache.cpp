#include "state_cache.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "state_codec.h"

namespace state_cache {

namespace {

constexpr const char *kPath = "/states.bin";
constexpr const char *kTmp = "/states.bin.tmp";
constexpr size_t kRtcCap = 4096;
constexpr size_t kFileMax = 64 * 1024;

RTC_DATA_ATTR uint8_t g_rtc[kRtcCap];
RTC_DATA_ATTR uint16_t g_rtcLen = 0;  // 0 = empty (zero after power-on)
int64_t g_epoch = 0;                  // of the restored record

bool fsReady() {
  static bool tried = false, ok = false;
  if (!tried) {
    tried = true;
    ok = LittleFS.begin(true);
  }
  return ok;
}

std::string readFile() {
  std::string data;
  if (!fsReady()) return data;
  File f = LittleFS.open(kPath, "r");
  if (!f) return data;
  const size_t n = f.size();
  if (n > 0 && n <= kFileMax) {
    data.assign(n, '\0');
    if (f.read(reinterpret_cast<uint8_t *>(&data[0]), n) != n) data.clear();
  }
  f.close();
  return data;
}

void writeFile(const std::string &data) {
  if (!fsReady() || data.size() > kFileMax) return;
  LittleFS.remove(kTmp);
  File f = LittleFS.open(kTmp, "w");
  if (!f) return;
  const size_t put = f.write(reinterpret_cast<const uint8_t *>(data.data()), data.size());
  f.close();
  if (put == data.size()) LittleFS.remove(kPath);
  if (put != data.size() || !LittleFS.rename(kTmp, kPath)) LittleFS.remove(kTmp);
}

}  // namespace

int64_t restore(const std::vector<std::string> &ids, StateStore &store) {
  const uint32_t t0 = millis();
  std::string rtc;
  if (g_rtcLen > 0 && g_rtcLen <= kRtcCap) rtc.assign(reinterpret_cast<const char *>(g_rtc), g_rtcLen);
  // Flash is read only when the RTC record is unusable (RTC is empty after power loss).
  int64_t epoch = 0;
  std::vector<EntityState> states;
  statecodec::Source src = statecodec::restore(rtc, std::string(), ids, epoch, states);
  if (src == statecodec::Source::None) src = statecodec::restore(std::string(), readFile(), ids, epoch, states);
  if (src != statecodec::Source::None) {
    store.update(states);
    g_epoch = epoch;
  }
  Serial.printf("STATECACHE: restore source=%s entities=%u epoch=%lld ms=%u\n",
                src == statecodec::Source::Rtc ? "rtc" : src == statecodec::Source::Flash ? "flash" : "none",
                (unsigned)states.size(), (long long)(src == statecodec::Source::None ? 0 : epoch),
                (unsigned)(millis() - t0));
  return src == statecodec::Source::None ? 0 : epoch;
}

void save(const StateStore &store, int64_t okEpoch) {
  if (store.size() == 0) return;
  const int64_t epoch = okEpoch > 0 ? okEpoch : g_epoch;
  const std::vector<EntityState> states = store.all();
  const std::string blob = statecodec::pack(states, epoch);
  const bool rtcOk = blob.size() <= kRtcCap;
  if (rtcOk) {
    memcpy(g_rtc, blob.data(), blob.size());
    g_rtcLen = uint16_t(blob.size());
  } else {
    g_rtcLen = 0;
  }
  // Flash only when the states differ from the stored record (the epoch alone does not count).
  bool same = false;
  const std::string old = readFile();
  int64_t oe = 0;
  std::vector<EntityState> os;
  if (!old.empty() && statecodec::unpack(old, oe, os)) same = os == states;
  if (!same) writeFile(blob);
  Serial.printf("STATECACHE: save entities=%u bytes=%u rtc=%d flash=%s\n", (unsigned)states.size(),
                (unsigned)blob.size(), rtcOk ? 1 : 0, same ? "unchanged" : "written");
}

}  // namespace state_cache
