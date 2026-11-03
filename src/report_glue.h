#pragma once
// Task 100: reporting the device's own sensors to HA. Arduino side of lib/device_report: the early SHT30 read,
// the "what was posted last" record in NVS, and the net command / result handling of one session.
#include <stdint.h>

#include "device_report.h"
#include "net_msgs.h"

namespace report_glue {

// Early in the wake (before Wi-Fi heats the chip): decides whether this wake reports (setting On, scheduled wake
// or cold boot, not demo) and if so reads the SHT30 once. Without reporting nothing is read. Idempotent per boot.
void wakeStart(bool settingOn, bool scheduled, bool coldBoot, devreport::WakeKind kind);

// The last SHT30 reading of this wake (for task 101's sleep screen). False if none was taken or it failed.
bool climate(int32_t &temp_x10, int32_t &hum_x10);

// A full state fetch ended. If a report is due (once per wake) and the fetch worked, fills `out` and returns true;
// the caller posts it. battery/voltage/charging come from the caller (they live in main.cpp).
bool onFetch(bool fetchOk, int32_t batteryPct, int32_t voltageMv, bool charging, int64_t nowEpoch,
             app_logic::NetCommand &out);

// The ReportDone event: marks what HA accepted, saves the record, logs `HA: report sent=N skipped=M ms=..`.
void onDone(const app_logic::NetEvent &ev, int64_t nowEpoch);

// A report is in flight (the session must not sleep yet).
bool busy();
// The session was restarted/abandoned: forget an in-flight report (its result is dropped with the old epoch).
void abort();

}  // namespace report_glue
