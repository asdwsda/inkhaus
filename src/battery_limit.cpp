#include "battery_limit.h"

#include <Arduino.h>
#include <esp_system.h>

#include "battery_learn.h"
#include "settings_nvs.h"
#include "wakelog.h"

namespace batterylimit {
namespace {

// UNVERIFIED: whether RTC memory survives a brownout reset on this board. The magic tells; the NVS copy
// (written at most once per wake, only on a >= 20 mV drop below 4.0 V) is the fallback.
constexpr uint32_t kMagic = 0xBA77E471u;
struct RtcLast {
  uint32_t magic;
  int32_t mv;
};
RTC_NOINIT_ATTR RtcLast g_rtcLast;

power::BatteryLearn g_st;
bool g_stored = false;  // NVS copy of last_mv already written in this wake

void save() {
  NvsSettingsStore store("batlearn");
  if (store.begin()) {
    power::save_learn(store, g_st);
    store.end();
  }
}

power::ResetKind kindOf(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_BROWNOUT: return power::ResetKind::Brownout;
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT:
    case ESP_RST_PANIC: return power::ResetKind::Crash;
    default: return power::ResetKind::Other;
  }
}

}  // namespace

void begin(bool learning, int32_t battery_mv) {
  {
    NvsSettingsStore store("batlearn");
    if (store.begin()) {
      g_st = power::load_learn(store);
      store.end();
    }
  }
  const bool rtcValid = g_rtcLast.magic == kMagic;
  const int32_t last = power::pick_last_mv(rtcValid, g_rtcLast.mv, g_st.last_mv);
  const esp_reset_reason_t rr = esp_reset_reason();
  bool dirty = false;
  if (power::is_battery_event(kindOf(rr), last, battery_mv)) {
    power::record_event(g_st, last);
    dirty = true;
    wakelog::logf("BOOT: battery_event_mv=%ld rtc_valid=%d\n", (long)last, rtcValid ? 1 : 0);
  }
  g_rtcLast.magic = kMagic;
  g_rtcLast.mv = battery_mv;
  if (power::on_sample(g_st, battery_mv, (int32_t)(millis() / 1000))) dirty = true;
  if (dirty) save();
  wakelog::logf("BOOT: battery_limit_mv=%ld learned=%d\n", (long)limit(learning), learned(learning) ? 1 : 0);
}

int32_t limit(bool learning) { return power::effective_limit(g_st, learning); }
bool learned(bool learning) { return power::is_learned(g_st, learning); }

void sample(int32_t battery_mv) {
  if (battery_mv <= 0) return;
  g_rtcLast.magic = kMagic;
  g_rtcLast.mv = battery_mv;
  bool dirty = power::on_sample(g_st, battery_mv, (int32_t)(millis() / 1000));
  if (!g_stored && power::should_store_last_mv(g_st.last_mv, battery_mv)) {
    g_st.last_mv = battery_mv;
    g_stored = true;
    dirty = true;
  }
  if (dirty) save();
}

}  // namespace batterylimit
