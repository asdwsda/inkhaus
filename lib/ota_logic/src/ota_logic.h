#pragma once
// Task 087: pure logic of the firmware self-update (no Arduino, no I/O).
#include <cstdint>
#include <string>

namespace ota_logic {

struct Version {
  int major = 0, minor = 0, patch = 0;
  bool pre = false;  // has a "-suffix"
};
// "1.2.3", "v1.2.3", "1.2" (patch 0), "1.3.0-rc1" (pre = true). False on anything else.
bool parseVersion(const std::string &s, Version &out);
// -1 / 0 / 1 on major.minor.patch (numeric: 1.10 > 1.9). Ignores pre.
int compare(const Version &a, const Version &b);
// True if `candidate` is a release (not pre-release) strictly newer than `current`.
bool isNewer(const std::string &current, const std::string &candidate);

struct Manifest {
  std::string version;
  std::string file;  // asset name, e.g. "firmware.bin"
  uint32_t size = 0;
  std::string sha256;  // 64 lowercase hex chars
};
// Reads manifest.json (task 061 + "ota" object). False if any field is missing or malformed.
bool parseManifest(const std::string &json, Manifest &out);
// Offer only when the version is a newer release and the size is plausible (0 < size <= maxSize).
// No suppression: a rolled-back version is offered again (installing needs an explicit Yes).
bool offer(const std::string &current, const Manifest &m, uint32_t maxSize);

// A manifest is small; anything above 8 KB (also when chunked, so the length is unknown) is rejected.
constexpr uint32_t kMaxManifestBytes = 8192;
// contentLen < 0: chunked / unknown (the reader caps at kMaxManifestBytes).
bool manifestLenOk(int64_t contentLen);

// TLS + download need one contiguous block; below this the check/download waits for the next eligible wake.
constexpr uint32_t kMinFreeBlock = 40 * 1024;
bool memoryOk(uint32_t largestFreeBlock);

// ---- check schedule ----
constexpr int64_t kCheckIntervalS = 7 * 86400;
constexpr int64_t kRetryIntervalS = 86400;
struct CheckState {
  int64_t lastAttempt = 0;  // epoch of the last attempt (0 = never)
  int64_t lastSuccess = 0;  // epoch of the last successful check
};
constexpr int64_t kColdCheckMinS = 3600;  // a cold boot checks at most once per hour (reboot loops)
// Check at a cold boot (at most once per hour), else when 7 days passed since the last success and (after a
// failure) 1 day since the last attempt. Never without Wi-Fi up. `nowEpoch` 0 (clock unset): only a cold boot checks.
bool checkDue(const CheckState &s, int64_t nowEpoch, bool coldBoot, bool wifiUp);

// Task 116: the automatic check never runs in an interactive session (it would hold the net path against taps);
// scheduled wakes only. Settings > Check now (userRequested) always may.
bool autoCheckAllowed(bool interactiveSession, bool userRequested);

// ---- install gate ----
enum class InstallBlock { None, Battery };
// Allowed on USB or battery >= 50 %.
InstallBlock installAllowed(bool usb, int batteryPct);

// ---- pending / health / rollback ----
constexpr int kMaxUnhealthyBoots = 3;
constexpr uint32_t kHealthyAfterMs = 10000;
// `earlierBoots` = boots of the new firmware that ended without a health mark (counted at the top of setup(), so
// a crash in setup counts too). True when that reached 3: the 4th start switches back to the previous slot.
bool rollbackNow(int earlierBoots);
// Healthy = setup finished, the first frame is drawn (so loop() runs) and it has been in loop() for 10 s without a
// crash. Network / HA state does not matter: a router outage must not roll back a good firmware.
bool healthy(bool firstFrameDrawn, uint32_t loopMs);

// ---- job deadline (task 090) ----
// Total wall-clock cap per network job; the per-step HTTP timeouts and the 15 s stall limit stay as they are.
constexpr uint32_t kCheckDeadlineMs = 60 * 1000;
constexpr uint32_t kInstallDeadlineMs = 5 * 60 * 1000;
// True once `elapsedMs` since the job start reached the cap of its kind (at the cap = expired).
bool jobExpired(bool install, uint32_t elapsedMs);

}  // namespace ota_logic
