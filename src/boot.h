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

// Plain "BOOT: key=value" lines.
void log(const char* key, const char* value);
void log(const char* key, long value);

}  // namespace boot
