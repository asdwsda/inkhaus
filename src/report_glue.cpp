#include "report_glue.h"

#include <Arduino.h>
#include <Preferences.h>

#include <vector>

#include "sht30_i2c.h"

namespace report_glue {

namespace {
bool g_wanted = false;       // this wake reports
bool g_planned = false;      // the report of this wake was built (once per wake, also across a session restart)
bool g_busy = false;
bool g_haveClimate = false;
sht30::Reading g_climate;
devreport::WakeKind g_kind = devreport::WakeKind::Boot;
devreport::Posted g_posted;
bool g_postedLoaded = false;
std::vector<devreport::Item> g_items;
bool g_lastWake = false;

constexpr const char *kNs = "devrep";
constexpr const char *kKey = "posted";
constexpr uint32_t kMagic = 0x44523031;  // "DR01": guards the record layout

struct Stored {
  uint32_t magic;
  devreport::Posted posted;
};

void loadPosted() {
  if (g_postedLoaded) return;
  g_postedLoaded = true;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  Stored s;
  if (p.getBytes(kKey, &s, sizeof s) == sizeof s && s.magic == kMagic) g_posted = s.posted;
  p.end();
}

void savePosted() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  const Stored s{kMagic, g_posted};
  p.putBytes(kKey, &s, sizeof s);
  p.end();
}
}  // namespace

void wakeStart(bool settingOn, bool scheduled, bool coldBoot, devreport::WakeKind kind) {
  g_wanted = devreport::sessionReports(settingOn, scheduled, coldBoot);
  g_kind = scheduled ? devreport::WakeKind::Scheduled : kind;
  g_lastWake = g_wanted;
  if (!g_wanted) return;
  const uint32_t t0 = millis();
  g_haveClimate = sht30_i2c::read(g_climate);
  Serial.printf("HA: sht30 ok=%d temp_x10=%d hum_x10=%d ms=%u\n", g_haveClimate ? 1 : 0,
                g_haveClimate ? (int)g_climate.temp_x10 : 0, g_haveClimate ? (int)g_climate.hum_x10 : 0,
                (unsigned)(millis() - t0));
}

bool climate(int32_t &temp_x10, int32_t &hum_x10) {
  if (!g_haveClimate) return false;
  temp_x10 = g_climate.temp_x10;
  hum_x10 = g_climate.hum_x10;
  return true;
}

bool onFetch(bool fetchOk, int32_t batteryPct, int32_t voltageMv, bool charging, int64_t nowEpoch,
             app_logic::NetCommand &out) {
  if (!g_wanted || g_planned || !fetchOk) return false;  // a failed fetch: the link is down, no retry in-session
  g_planned = true;
  loadPosted();
  devreport::Readings r;
  r.battery_pct = batteryPct;
  r.voltage_mv = voltageMv;
  r.charging = charging;
  r.have_climate = g_haveClimate;
  r.temp_x10 = g_climate.temp_x10;
  r.hum_x10 = g_climate.hum_x10;
  g_items = devreport::plan(r, g_posted, nowEpoch, g_lastWake, g_kind);
  if (g_items.empty()) {
    Serial.printf("HA: report sent=0 skipped=0 ms=0 due=0\n");
    return false;
  }
  out = app_logic::NetCommand();
  out.type = app_logic::NetCommand::Type::Report;
  for (const auto &it : g_items) out.reports.emplace_back(it.entity, it.body);
  g_busy = true;
  return true;
}

void onDone(const app_logic::NetEvent &ev, int64_t nowEpoch) {
  g_busy = false;
  for (size_t i = 0; i < g_items.size() && i < ev.reportOk.size(); ++i)
    if (ev.reportOk[i]) devreport::markPosted(g_posted, g_items[i], nowEpoch);
  if (ev.reportSent) savePosted();
  Serial.printf("HA: report sent=%u skipped=%u ms=%u err=%d\n", (unsigned)ev.reportSent, (unsigned)ev.reportSkipped,
                (unsigned)ev.ms, (int)ev.error);
  g_items.clear();
}

bool busy() { return g_busy; }

void abort() {
  if (!g_busy) return;  // already done (or nothing due): never report twice in a wake
  g_busy = false;
  g_items.clear();
  g_planned = false;  // the in-flight report was lost with the old session: the restarted one may try once more
}

}  // namespace report_glue
