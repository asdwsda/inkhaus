#pragma once
// Task 087: firmware self-update. Logic (versions, schedule, health) is lib/ota_logic; this file is the
// hardware side (HTTPS, esp_ota, NVS namespace "ota") and the small interface the Settings UI uses.
// NVS keys: last_try, last_ok (epoch), avail, avail_sz (offer), pend (0/1), boots (boots without health mark),
// from (version before the update), rb (rolled-back notice).
// Trust model: GitHub (latest release of asdwsda/inkhaus) + TLS (embedded CA bundle) + the SHA-256 in the release
// manifest. There is no firmware signature: whoever controls the release controls the device.
#include <stdint.h>

#include <string>

#include "ota_logic.h"

namespace ota {

// ---- boot / loop hooks (src/main.cpp) ----
// FIRST call of setup(), before anything that can crash: counts a boot of a freshly installed firmware; on the 4th
// start without a health mark it switches back to the previous slot and restarts (does not return then).
void earlyBoot();
void logEarly();  // print earlyBoot()'s result; call once Serial is up (after boot::begin())
// After the first frame: loads the state. coldBoot = the splash path.
void begin(bool coldBoot);
// Called before a due check / a download: close the HA socket (frees ~40 KB of heap). May block briefly.
void setQuiesce(void (*fn)());
// Every loop iteration (cheap). Marks the firmware healthy after 10 s in loop(); runs the due check (blocking, a
// few seconds) only when Wi-Fi is up and the HA session is done. epoch 0 = clock unset.
// interactive: automatic check only after kAutoIdleMs without input and with an empty user lane (task 116).
void tick(uint32_t nowMs, int64_t epoch, bool wifiUp, bool sessionDone, bool interactive, uint32_t msSinceInput, bool userLaneBusy);
// Task 116: the automatic check runs in the net task. tick() only plans it: it calls the poster (enqueues the job in
// the net task's background lane; returns false if it cannot) and later picks up the result state. The net task
// calls runAutoCheckJob(); the HA socket is closed by it before.
void setPoster(bool (*fn)());
void runAutoCheckJob();  // net task: blocking, yields to a waiting user command (then tick() retries when idle)
// The automatic check aborts (cancelled) while this returns true (a user command waits). Task 116.
void setYield(bool (*fn)());
// Before deep sleep / power off: a session that drew its first frame and then slept on purpose was not a crash.
void beforeSleep();

// ---- UI interface (Settings, Tools page) ----
const char *currentVersion();            // FW_VERSION
bool available();                        // a newer, not suppressed release was found (persisted)
const std::string &availableVersion();   // "1.3.0" or ""
uint32_t availableSize();                // bytes of the offered image (from the manifest), 0 unknown
int64_t lastCheckEpoch();                // last successful check, 0 never
// Notices for the first Settings visit: "1.2.1" after an update, "1.3.0" after a rollback, else "".
const std::string &updatedFrom();
const std::string &rolledBack();
void clearNotices();                     // when Settings is left
// Why an install is refused right now (Battery), None if allowed.
ota_logic::InstallBlock installBlock(bool usb, int batteryPct);
// True while a check or download runs: the caller must not idle/wake-sleep.
bool busy();

enum class CheckResult { Newer, UpToDate, NoWifi, NoServer, Timeout, Skipped };
// "Check now": blocking (up to ~20 s). Records the result like the weekly check.
CheckResult checkNow(int64_t epoch, bool wifiUp);

enum class Phase { Downloading, Verifying, Restarting };
enum class Failure { None, Download, Damaged, TooLong };
// Blocking install: download into the inactive slot, SHA-256 check, set boot partition, restart. progress is
// called about every 2 %. Returns only on failure (running firmware untouched); failure() says why.
bool install(void (*progress)(Phase phase, int pct));
Failure failure();
const char *lastError();  // "" if none

}  // namespace ota
