#pragma once
// Real sleep and wake (task 024). Hardware side; the decisions are in lib/app_logic/sleep_logic.
// Log lines: "WAKE: key=value ..." at boot, "SLEEP: key=value ..." before sleeping.
#include <stdint.h>

#include "power.h"
#include "sleep_logic.h"

namespace sleepwake {

// Call once after boot::begin() (M5.begin() done): maps the reset reason, the wake pin, the BM8563
// alarm flag and the "we slept" marker to a wake reason, then clears the marker and the RTC alarm
// (an alarm that is still armed must not fire during the session) and releases the GPIO hold of
// a deep sleep. Logs "WAKE: reason=...".
power::WakeReason detect();

// Task 085: GPIO3 pull-down probe of the CH9102 TX line (1 = USB present, 0 = battery), 3 ms, pull-up restored.
int probeUsbRx();

// Valid after detect(). wokeFromSleep: the frozen panel image is ours (deep-sleep or power-off wake): keep it.
// splashWanted: reset button or a real cold boot.
bool wokeFromSleep();
bool splashWanted();

// The indicator variant that is on the panel from the previous sleep (None after a cold boot).
// Valid after detect(); only meaningful for a scheduled-refresh wake (the first frame then keeps it).
app_logic::Indicator storedIndicator();

// The pill that was drawn before the previous sleep (task 085): its indicator and the epoch of the next scheduled
// update shown on line 2 (0 = "off"). Valid after detect(); None after a cold boot with nothing stored.
app_logic::Indicator panelIndicator();
int64_t panelNextAt();

// True if the panel still shows the sleep badge of our previous deep sleep (any wake kind except a power-up
// or reset). Valid after detect(). Task 058: the early wake feedback clears it.
bool badgeOnPanel();

// Task 086: the tab shown at the last sleep (NVS, so it survives deep sleep and power-off). saveTab writes only
// when the value changed. UNVERIFIED on the device.
int savedTab();
void saveTab(int tab);

// Last step of a session. Persists the marker and the indicator variant, arms the BM8563 alarm, then
// either releases the main power hold (PowerOff; on battery the device is off after this call) or
// enters ESP32 deep sleep (Touch mode, and the USB-powered fallback of PowerOff). Does not return
// unless something prevented sleeping. The caller has drawn the indicator, closed the controller
// read state (boot::endBurstRead) and stopped Wi-Fi.
void enter(const power::SleepPlan &plan, power::Session session, int64_t nowEpoch, int32_t batteryMv);

}  // namespace sleepwake
