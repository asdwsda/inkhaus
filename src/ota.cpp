#include "ota.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "boot.h"
#include "version.h"
#include "wakelog.h"

extern const uint8_t ca_bundle_start[] asm("_binary_certs_x509_crt_bundle_start") __attribute__((weak));

namespace ota {
namespace {

#define OTA_RELEASE_BASE "https://github.com/asdwsda/inkhaus/releases/latest/download/"

std::string g_avail;
std::string g_err;
bool g_busy = false;
bool g_pending = false;
bool g_healthyDone = false;
bool g_checkedThisBoot = false;
bool g_cold = false;
bool g_began = false;  // first frame drawn
bool g_loopSeen = false;
uint32_t g_loopStartMs = 0;
uint32_t g_availSize = 0;
std::string g_from, g_updatedFrom, g_rolledBack;
Failure g_failure = Failure::None;
CheckResult g_failKind = CheckResult::NoServer;
void (*g_quiesce)() = nullptr;
ota_logic::CheckState g_sched;

std::string baseUrl() {
#ifdef DEBUG_SERIAL
  // Debug builds only: a local test server (tools/ota_test_server.py). Build flag or NVS ota/url,
  // e.g. "http://192.168.1.10:8070/". Release builds always use GitHub.
#ifdef OTA_TEST_URL
  return OTA_TEST_URL;
#else
  Preferences p;
  if (p.begin("ota", true)) {
    String u = p.getString("url", "");
    p.end();
    if (u.length()) return u.c_str();
  }
#endif
#endif
  return OTA_RELEASE_BASE;
}

void loadState() {
  Preferences p;
  if (!p.begin("ota", true)) return;
  g_sched.lastAttempt = (int64_t)p.getUInt("last_try", 0);
  g_sched.lastSuccess = (int64_t)p.getUInt("last_ok", 0);
  g_avail = p.getString("avail", "").c_str();
  g_availSize = p.getUInt("avail_sz", 0);
  g_from = p.getString("from", "").c_str();
  g_rolledBack = p.getString("rb", "").c_str();
  p.end();
  // A stored offer that is no longer newer (we were updated) is dropped.
  if (!g_avail.empty() && (!ota_logic::isNewer(FW_VERSION, g_avail))) g_avail.clear();
  if (!g_from.empty() && g_from != FW_VERSION) g_updatedFrom = g_from;
}

void saveCheck(bool ok, int64_t epoch) {
  Preferences p;
  if (!p.begin("ota", false)) return;
  p.putUInt("last_try", (uint32_t)epoch);
  if (ok) {
    p.putUInt("last_ok", (uint32_t)epoch);
    p.putString("avail", g_avail.c_str());
    p.putUInt("avail_sz", g_availSize);
  }
  p.end();
}

void setPending(bool on) {
  Preferences p;
  if (!p.begin("ota", false)) return;
  p.putUChar("pend", on ? 1 : 0);
  p.putUChar("boots", 0);
  p.end();
  g_pending = on;
}

// Opens url on `http` (plain or https with the embedded bundle), follows redirects (GitHub asset -> CDN).
bool open(HTTPClient &http, WiFiClientSecure &tls, WiFiClient &plain, const std::string &url) {
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(5);
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.setUserAgent("inkhaus-ota/" FW_VERSION);
  if (url.rfind("https://", 0) == 0) {
    if (!ca_bundle_start) {
      g_err = "no CA bundle";
      return false;
    }
    tls.setCACertBundle(ca_bundle_start);
    if (!http.begin(tls, url.c_str())) {
      g_err = "begin failed";
      return false;
    }
  } else if (!http.begin(plain, url.c_str())) {
    g_err = "begin failed";
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    g_err = "http " + std::to_string(code);
    g_failKind = code == HTTPC_ERROR_READ_TIMEOUT ? CheckResult::Timeout : CheckResult::NoServer;
    http.end();
    return false;
  }
  return true;
}

uint32_t maxImageSize() {
  const esp_partition_t *p = esp_ota_get_next_update_partition(nullptr);
  return p ? p->size : 0;
}

// Task 090: total deadline per job. Cooperative: the job checks cancelled() at its read steps (no task kill); the
// loop also evaluates it while waiting, so the log line is written once whoever sees it first.
uint32_t g_jobStartMs = 0;
bool g_jobInstall = false;
volatile bool g_timedOut = false;
bool cancelled() {
  if (g_timedOut) return true;
  const uint32_t el = millis() - g_jobStartMs;
  if (!ota_logic::jobExpired(g_jobInstall, el)) return false;
  g_timedOut = true;
  wakelog::logf("OTA: job timeout kind=%s after_ms=%u\n", g_jobInstall ? "install" : "check", (unsigned)el);
  return true;
}

// Collects at most kMaxManifestBytes; a longer body makes write() fail, which aborts the transfer.
class CapStream : public Stream {
 public:
  std::string data;
  bool overflow = false;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *buf, size_t n) override {
    if (cancelled()) return 0;  // deadline passed: aborts the transfer
    if (data.size() + n > ota_logic::kMaxManifestBytes) {
      overflow = true;
      return 0;
    }
    data.append((const char *)buf, n);
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
};

bool fetchManifest(ota_logic::Manifest &m) {
  WiFiClientSecure tls;
  WiFiClient plain;
  HTTPClient http;
  if (!open(http, tls, plain, baseUrl() + "manifest.json")) return false;
  if (!ota_logic::manifestLenOk(http.getSize())) {
    g_err = "manifest too big";
    g_failKind = CheckResult::NoServer;
    http.end();
    return false;
  }
  CapStream body;  // chunked bodies have no length: the cap is enforced while reading
  http.writeToStream(&body);
  http.end();
  if (g_timedOut) {
    g_err = "job timeout";
    g_failKind = CheckResult::Timeout;
    return false;
  }
  if (body.overflow) {
    g_err = "manifest too big";
    g_failKind = CheckResult::NoServer;
    return false;
  }
  if (!ota_logic::parseManifest(body.data, m)) {
    g_err = "bad manifest";
    g_failKind = CheckResult::NoServer;
    return false;
  }
  return true;
}

// Frees the HA socket, then needs a contiguous block for TLS. False = skip to the next eligible wake.
bool prepareHeap() {  // runs inside the job task, after its stack was allocated
  const uint32_t blk = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (!ota_logic::memoryOk(blk)) {
    g_err = "low memory";
    wakelog::logf("OTA: skipped, largest free block %u\n", (unsigned)blk);
    return false;
  }
  return true;
}

// Returns false if skipped for memory (nothing recorded: the next eligible wake tries again).
bool runCheckBody(int64_t epoch, CheckResult &result) {
  g_err.clear();
  g_failKind = CheckResult::NoServer;
  if (!prepareHeap()) {
    result = CheckResult::Skipped;
    return false;
  }
  ota_logic::Manifest m;
  bool ok = fetchManifest(m);
  if (ok) {
    g_avail = ota_logic::offer(FW_VERSION, m, maxImageSize()) ? m.version : std::string();
    g_availSize = g_avail.empty() ? 0 : m.size;
    result = g_avail.empty() ? CheckResult::UpToDate : CheckResult::Newer;
    wakelog::logf("OTA: check ok latest=%s offer=%d\n", m.version.c_str(), (int)!g_avail.empty());
  } else {
    result = g_failKind;
    wakelog::logf("OTA: check failed err=%s\n", g_err.c_str());  // silent for the user
  }
  g_sched.lastAttempt = epoch;
  if (ok) g_sched.lastSuccess = epoch;
  saveCheck(ok, epoch);
  return true;
}

void rollback() {
  const esp_partition_t *prev = esp_ota_get_next_update_partition(nullptr);
  {
    Preferences p;
    if (p.begin("ota", false)) {  // the failed version is only noted (rb); it is offered again
      p.putString("rb", FW_VERSION);
      p.remove("from");
      p.end();
    }
  }
  setPending(false);
  if (prev && esp_ota_set_boot_partition(prev) == ESP_OK) {
    Serial.begin(115200);  // earlyBoot() runs before boot::begin(): Serial is not up yet
    Serial.println("OTA: rollback to previous slot (3 boots without health mark)");
    Serial.flush();
    boot::endBurstRead();
    ESP.restart();
  }
  Serial.begin(115200);
  Serial.println("OTA: rollback failed, previous slot not bootable");
}

char g_early[48] = "";  // earlyBoot() result, printed by logEarly() once Serial is up

void markHealthy() {
  if (!g_pending || g_healthyDone) return;
  g_healthyDone = true;
  setPending(false);
  wakelog::logf("OTA: healthy, pending cleared\n");
}


// Task 089: network jobs run in a one-shot task with a 20 KB stack (the TLS handshake overflows the loop stack).
// The loop only polls this struct; it does not touch HTTPClient/Update while the job runs.
constexpr uint32_t kJobStack = 20480;
struct Job {
  enum Kind { Check, Install } kind = Check;
  int64_t epoch = 0;
  CheckResult result = CheckResult::Skipped;
  bool checkRan = false;  // false = skipped for memory
  bool installOk = false;
  volatile bool done = false;
  volatile int phase = -1;  // Phase, -1 none yet
  volatile int pct = 0;
  volatile uint32_t stackFree = 0;
};
Job *g_job = nullptr;

void jobProgress(Phase ph, int pct) {
  g_job->pct = pct;
  g_job->phase = (int)ph;
}

bool installBody(void (*progress)(Phase, int));

void jobEntry(void *arg) {
  Job *j = (Job *)arg;
  if (j->kind == Job::Check) {
    j->checkRan = runCheckBody(j->epoch, j->result);
  } else {
    j->installOk = installBody(&jobProgress);
  }
  j->stackFree = uxTaskGetStackHighWaterMark(nullptr);
  j->done = true;
  vTaskDelete(nullptr);
}

// Blocking for the caller: starts the task, polls it, forwards install progress from the caller's thread.
// False = the task could not be created (memory); g_err is set.
bool runJob(Job &j, void (*progress)(Phase, int)) {
  g_job = &j;
  g_jobStartMs = millis();
  g_jobInstall = j.kind == Job::Install;
  g_timedOut = false;
  TaskHandle_t h = nullptr;
  if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < kJobStack + ota_logic::kMinFreeBlock ||
      xTaskCreate(&jobEntry, "otajob", kJobStack, &j, 1, &h) != pdPASS) {
    g_err = "low memory";
    wakelog::logf("OTA: skipped, cannot start job task\n");
    g_job = nullptr;
    return false;
  }
  int lastPhase = -1, lastPct = -1;
  while (!j.done) {
    cancelled();  // flags the job; it aborts at its next step. We still wait for `done` before freeing anything
    const int ph = j.phase, pc = j.pct;
    if (progress && ph >= 0 && (ph != lastPhase || pc != lastPct)) {
      lastPhase = ph;
      lastPct = pc;
      progress((Phase)ph, pc);
    }
    delay(20);
  }
  const int ph = j.phase;
  if (progress && ph >= 0 && (ph != lastPhase || j.pct != lastPct)) progress((Phase)ph, j.pct);
  wakelog::logf("APP: ota_stack_free=%u\n", (unsigned)j.stackFree);
  g_job = nullptr;
  return true;
}

bool runCheck(int64_t epoch, CheckResult &result) {
  g_err.clear();
  if (g_quiesce) g_quiesce();  // net-task call: from the loop, before the job task exists
  Job j;
  j.kind = Job::Check;
  j.epoch = epoch;
  if (!runJob(j, nullptr)) {
    result = CheckResult::Skipped;
    return false;
  }
  result = j.result;
  return j.checkRan;
}
}  // namespace

void earlyBoot() {
  Preferences p;
  if (!p.begin("ota", false)) return;
  g_pending = p.getUChar("pend", 0) != 0;
  if (g_pending) {
    const int earlier = p.getUChar("boots", 0);  // boots of this firmware without a health mark so far
    snprintf(g_early, sizeof g_early, "OTA: pending boots_before=%d\n", earlier);
    if (ota_logic::rollbackNow(earlier)) {
      p.end();
      rollback();
      return;
    }
    p.putUChar("boots", (uint8_t)(earlier + 1));
  }
  p.end();
}

void logEarly() {
  if (g_early[0]) Serial.print(g_early);
  g_early[0] = 0;
}

void begin(bool coldBoot) {
  g_cold = coldBoot;
  g_began = true;
  loadState();
  if (!g_rolledBack.empty()) wakelog::logf("OTA: rolled back, failed version %s\n", g_rolledBack.c_str());
}

void setQuiesce(void (*fn)()) { g_quiesce = fn; }

void tick(uint32_t nowMs, int64_t epoch, bool wifiUp, bool sessionDone) {
  if (g_busy) return;
  if (!g_loopSeen) {
    g_loopSeen = true;
    g_loopStartMs = nowMs;
  }
  if (g_pending && !g_healthyDone && ota_logic::healthy(g_began, nowMs - g_loopStartMs)) markHealthy();
  if (g_checkedThisBoot || !wifiUp || !sessionDone) return;
  if (!ota_logic::checkDue(g_sched, epoch, g_cold, wifiUp)) return;
  g_checkedThisBoot = true;  // one attempt per boot/wake; failures retry at the next eligible wake
  g_busy = true;
  CheckResult r;
  runCheck(epoch, r);
  g_busy = false;
}

void beforeSleep() {
  if (g_began) markHealthy();  // a deliberate sleep after the first frame is not a crash
}

CheckResult checkNow(int64_t epoch, bool wifiUp) {
  if (g_busy) return CheckResult::Skipped;
  if (!wifiUp) return CheckResult::NoWifi;
  g_busy = true;
  CheckResult r = CheckResult::Skipped;
  runCheck(epoch, r);
  g_busy = false;
  return r;
}

const std::string &updatedFrom() { return g_updatedFrom; }
const std::string &rolledBack() { return g_rolledBack; }
void clearNotices() {
  if (g_updatedFrom.empty() && g_rolledBack.empty()) return;
  g_updatedFrom.clear();
  g_rolledBack.clear();
  g_from.clear();
  Preferences p;
  if (p.begin("ota", false)) {
    p.remove("from");
    p.remove("rb");
    p.end();
  }
}
uint32_t availableSize() { return g_availSize; }
int64_t lastCheckEpoch() { return g_sched.lastSuccess; }
Failure failure() { return g_failure; }

const char *currentVersion() { return FW_VERSION; }
bool available() { return !g_avail.empty(); }
const std::string &availableVersion() { return g_avail; }
bool busy() { return g_busy; }
const char *lastError() { return g_err.c_str(); }

ota_logic::InstallBlock installBlock(bool usb, int batteryPct) { return ota_logic::installAllowed(usb, batteryPct); }

namespace {
// Install job body (runs in the job task): everything up to and including set_boot_partition.
bool installBody(void (*progress)(Phase, int)) {
  bool ok = false;
  esp_ota_handle_t handle = 0;
  bool handleOpen = false;
  do {
    if (!prepareHeap()) break;
    ota_logic::Manifest m;
    if (!fetchManifest(m)) {
      if (g_timedOut) g_failure = Failure::TooLong;
      break;
    }
    if (!ota_logic::offer(FW_VERSION, m, maxImageSize())) {
      g_err = "no newer version";
      break;
    }
    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (!part) {
      g_err = "no slot";
      break;
    }
    WiFiClientSecure tls;
    WiFiClient plain;
    HTTPClient http;
    if (!open(http, tls, plain, baseUrl() + m.file)) break;
    const int len = http.getSize();
    if (len > 0 && (uint32_t)len != m.size) {
      g_err = "size mismatch";
      http.end();
      break;
    }
    if (esp_ota_begin(part, m.size, &handle) != ESP_OK) {
      g_err = "ota_begin failed";
      http.end();
      break;
    }
    handleOpen = true;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    WiFiClient *s = http.getStreamPtr();
    uint8_t buf[1024];
    uint32_t got = 0;
    int lastPct = -1;
    uint32_t lastData = millis();
    bool failed = false;
    if (progress) progress(Phase::Downloading, 0);
    while (got < m.size) {
      const int avail = s->available();
      if (avail <= 0) {
        if (cancelled()) {
          failed = true;
          break;
        }
        if (!s->connected() || millis() - lastData > 15000) {
          g_err = "download stalled";
          failed = true;
          break;
        }
        delay(2);
        continue;
      }
      const int n = s->readBytes(buf, std::min<size_t>(sizeof(buf), std::min<uint32_t>(avail, m.size - got)));
      if (n <= 0) continue;
      lastData = millis();
      if (cancelled()) {
        failed = true;
        break;
      }
      mbedtls_sha256_update(&sha, buf, n);
      if (esp_ota_write(handle, buf, n) != ESP_OK) {
        g_err = "flash write failed";
        failed = true;
        break;
      }
      got += n;
      const int pct = (int)((uint64_t)got * 100 / m.size);
      if (progress && pct >= lastPct + 2) {
        lastPct = pct;
        progress(Phase::Downloading, pct);
      }
    }
    http.end();
    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    if (failed && g_timedOut) {
      g_err = "job timeout";
      g_failure = Failure::TooLong;
      break;
    }
    if (failed) break;
    static const char *hex = "0123456789abcdef";
    std::string got256;
    for (uint8_t b : digest) {
      got256 += hex[b >> 4];
      got256 += hex[b & 15];
    }
    if (got256 != m.sha256) {
      g_err = "sha256 mismatch";
      g_failure = Failure::Damaged;
      break;
    }
    handleOpen = false;
    if (esp_ota_end(handle) != ESP_OK) {  // validates the image header and segments
      g_err = "image invalid";
      break;
    }
    if (progress) progress(Phase::Verifying, 100);
    setPending(true);
    {
      Preferences p;
      if (p.begin("ota", false)) {
        p.putString("from", FW_VERSION);
        p.end();
      }
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
      setPending(false);
      g_err = "set boot failed";
      break;
    }
    if (progress) progress(Phase::Restarting, 100);
    g_failure = Failure::None;
    ok = true;
  } while (false);
  if (handleOpen) esp_ota_abort(handle);
  return ok;
}
}  // namespace

bool install(void (*progress)(Phase, int)) {
  g_err.clear();
  g_failure = Failure::Download;
  if (g_busy) return false;
  g_busy = true;
  if (g_quiesce) g_quiesce();  // net-task call: from the loop, before the job task exists
  Job j;
  j.kind = Job::Install;
  const bool ok = runJob(j, progress) && j.installOk;
  g_busy = false;
  if (ok) {
    Serial.println("OTA: installed, restarting");
    Serial.flush();
    boot::endBurstRead();  // the controller must be idle before a restart
    delay(200);
    ESP.restart();
  }
  wakelog::logf("OTA: install failed err=%s\n", g_err.c_str());
  return false;
}

}  // namespace ota
