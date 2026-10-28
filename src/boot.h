#pragma once
// The one place that brings up the hardware. Every log line is "BOOT: <key>=<value>".
#include <stdint.h>

namespace boot {

enum class Start { NoReset, Stock };

// Serial.begin(115200), then display start-up and M5.begin() with the app's config.
//
// Display start-up, first choice (NoReset): probe the IT8951 with GET_DEV_INFO without pulsing
// its reset line, then hand M5.Display a hand-built Panel_IT8951 + Touch_GT911 (same settings as
// M5GFX's M5Paper autodetect) and mark it as the M5Paper board, so M5.begin() skips its own
// autodetect (which hardcodes the reset pulse) and sets up power, buttons, RTC and touch as usual.
// Fallback (Stock): if the controller does not answer or reports another panel size, M5.begin()
// runs the stock autodetect including the reset pulse.
void begin();

Start startMode();

// True when the IT8951 had just powered up (hardware reset), so its image memory holds nothing
// from the previous run. Only known for Start::NoReset; Stock reports false.
bool controllerWasCold();

// Logs "BOOT: t_<stage>_ms=<ms since boot::begin() started>".
void mark(const char* stage);

// Logs reset reason and battery voltage (mV).
void logHealth();

// Milliseconds since boot::begin() started.
uint32_t elapsedMs();

// ---- controller protocol and bounded waits (task 016) --------------------------------------------

// Polls the IT8951 BUSY pin (high = ready) for at most `timeoutMs`. Logs
// "BOOT: busy_timeout=<what>" on failure.
bool waitBusyHigh(uint32_t timeoutMs, const char* what);

// Sends MEM_BST_END. Every memory read (MEM_BST_RD_S) opens a burst that the controller keeps
// open until this command; Panel_IT8951::readRect never sends it. Call it after each readRectRGB.
// Bounded; false if the controller did not accept it.
bool endBurstRead();

// Puts the IT8951 into STANDBY (keeps image memory) as the last display operation before a deep
// sleep (closes a read burst first). The next boot::begin() after ESP_RST_DEEPSLEEP sends SYS_RUN
// before probing; a controller that then does not answer takes the stock path (cold). Bounded waits.
bool controllerStandby();

// Bounded check that the controller answers (BUSY high). If it does not, resets it (hardware
// reset pulse, full panel init) and reports it as cold. Returns true when no recovery was needed.
bool ensureControllerReady(const char* where);

// ---- watchdog ----------------------------------------------------------------------------------

// True when this boot follows >= 2 consecutive watchdog resets/panics: the stock display start-up
// is used, and the caller should draw the whole screen.
bool fallbackActive();

// Arms the task watchdog for the calling task (boot::begin() does it). Feed it from long stages.
void feed();

// Call at the end of setup(): disarms the setup watchdog and clears the crash counter.
void setupDone();

// Plain "BOOT: key=value" lines.
void log(const char* key, const char* value);
void log(const char* key, long value);

}  // namespace boot
