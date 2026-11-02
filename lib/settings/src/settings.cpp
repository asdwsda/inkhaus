#include "settings.h"

#include <algorithm>

namespace settings {

namespace {
template <typename Arr>
bool contains(const Arr& arr, int32_t v) {
    return std::find(arr.begin(), arr.end(), v) != arr.end();
}

void writeIfChanged(KeyValueStore& store, const char* key, int32_t value) {
    int32_t cur;
    if (store.getInt(key, cur) && cur == value) return;
    store.setInt(key, value);
}
}  // namespace

bool isValidSleepMode(int32_t v) {
    return v >= static_cast<int32_t>(SleepSetting::PowerOff) && v <= static_cast<int32_t>(SleepSetting::TouchAndRocker);
}
bool isValidKeepAwake(int32_t v) {
    return v >= static_cast<int32_t>(KeepAwake::Off) && v <= static_cast<int32_t>(KeepAwake::Always);
}

int32_t snapRefreshIntervalMin(int32_t v) {
    if (isValidRefreshIntervalMin(v)) return v;
    int32_t best = kRefreshIntervalMinValues[0];
    for (int32_t c : kRefreshIntervalMinValues) {
        const int32_t dc = c > v ? c - v : v - c, db = best > v ? best - v : v - best;
        if (dc < db || (dc == db && c > best)) best = c;
    }
    return best;
}

bool keepAwakeOnUsbOffered(SleepSetting m, const Capabilities& c) {
    return m == SleepSetting::PowerOff || c.usb_probe_reliable;
}
bool updatesForcedOff(const Settings& s, const Capabilities& c) {
    return s.sleep_mode == SleepSetting::PowerOff && !c.rtc_power_on;
}
int32_t effectiveRefreshMin(const Settings& s, const Capabilities& c) {
    return updatesForcedOff(s, c) ? 0 : s.refresh_interval_min;
}
KeepAwake effectiveKeepAwake(const Settings& s, const Capabilities& c) {
    return s.keep_awake == KeepAwake::OnUsb && !keepAwakeOnUsbOffered(s.sleep_mode, c) ? KeepAwake::Off : s.keep_awake;
}
Settings normalizeSleepFields(Settings s, const Capabilities& c) {
    s.keep_awake = effectiveKeepAwake(s, c);
    return s;
}
bool isValidRefreshIntervalMin(int32_t v) { return contains(kRefreshIntervalMinValues, v); }
bool isValidIdleTimeoutS(int32_t v) { return contains(kIdleTimeoutSValues, v); }

bool isValidOrientation(int32_t v) {
    return v == static_cast<int32_t>(Orientation::Portrait) ||
           v == static_cast<int32_t>(Orientation::Landscape);
}

bool isValidDemoMode(int32_t v) { return v == 0 || v == 1; }

bool isValidDebugLog(int32_t v) { return v == 0 || v == 1; }

bool isValidWifiProfileIndex(int32_t v) { return v >= 0 && v <= kMaxWifiProfileIndex; }

bool isValidLearnBattery(int32_t v) { return v == 0 || v == 1; }

bool isValidHaLink(int32_t v) { return v >= 0 && v <= static_cast<int32_t>(HaLink::External); }

int wifiProfileIndex(const std::string& name, const std::vector<std::string>& names) {
    if (name.empty()) return 0;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == name) return static_cast<int>(i) + 1;
    return 0;
}

bool migrateWifiProfile(Settings& s, const std::vector<std::string>& names) {
    if (s.legacy_wifi_index <= 0) return false;
    const size_t i = static_cast<size_t>(s.legacy_wifi_index);
    s.wifi_profile = i <= names.size() ? names[i - 1] : std::string();
    s.legacy_wifi_index = 0;
    return true;
}

Settings load(const KeyValueStore& store) {
    Settings s;
    int32_t v;
    // A missing version counts as the legacy schema 1.
    int32_t ver = 1;
    if (store.getInt(kKeySchemaVersion, v)) ver = v;
    if (ver != kSchemaVersion && ver != 2 && ver != 1) return s;
    if (ver == kSchemaVersion) {
        if (store.getInt(kKeySleepMode, v) && isValidSleepMode(v)) s.sleep_mode = static_cast<SleepSetting>(v);
        if (store.getInt(kKeyKeepAwake, v) && isValidKeepAwake(v)) s.keep_awake = static_cast<KeepAwake>(v);
    } else {
        // Migration (task 085). Old wake modes: 0 Button, schema 2: 1 Touch, 2 Touch & Button; schema 1: 1 and 2 both.
        // Button -> Deep sleep: every sleep so far was a deep sleep (rocker wake).
        if (store.getInt(kKeyWakeMode, v) && v >= 0 && v <= 2) {
            if (v == 0) s.sleep_mode = SleepSetting::DeepSleep;
            else if (v == 1 && ver == 2) s.sleep_mode = SleepSetting::Touch;
            else s.sleep_mode = SleepSetting::TouchAndRocker;
        }
        if (store.getInt(kKeyStayAwakeUsb, v) && v == 1) s.keep_awake = KeepAwake::OnUsb;
    }
    if (store.getInt(kKeyRefreshMin, v)) {
        if (ver == kSchemaVersion) {
            if (isValidRefreshIntervalMin(v)) s.refresh_interval_min = v;
        } else if (v == 0 || v == 15 || v == 30 || v == 60 || v == 120 || v == 240) {
            s.refresh_interval_min = snapRefreshIntervalMin(v);  // the schema 1/2 values: 15/120/240 are gone
        }
    }
    if (store.getInt(kKeyIdleTimeoutS, v) && isValidIdleTimeoutS(v)) s.idle_timeout_s = v;
    if (store.getInt(kKeyOrientation, v) && isValidOrientation(v))
        s.orientation = static_cast<Orientation>(v);
    if (store.getInt(kKeyDemoMode, v) && isValidDemoMode(v)) s.demo_mode = v == 1;
    if (store.getInt(kKeyDebugLog, v) && isValidDebugLog(v)) s.debug_log = v == 1;
    std::string name;
    if (store.getStr(kKeyWifiProfile, name)) {
        s.wifi_profile = name;
    } else if (store.getInt(kKeyWifiProfileLegacy, v) && isValidWifiProfileIndex(v)) {
        s.legacy_wifi_index = v;  // resolved by migrateWifiProfile() once the profile list is known
    }
    if (store.getInt(kKeyHaLink, v) && isValidHaLink(v)) s.ha_link = static_cast<HaLink>(v);
    if (store.getInt(kKeyLearnBattery, v) && isValidLearnBattery(v)) s.learn_battery = v == 1;
    return s;
}

void save(KeyValueStore& store, const Settings& s) {
    writeIfChanged(store, kKeySchemaVersion, kSchemaVersion);
    writeIfChanged(store, kKeySleepMode, static_cast<int32_t>(s.sleep_mode));
    writeIfChanged(store, kKeyRefreshMin, s.refresh_interval_min);
    writeIfChanged(store, kKeyIdleTimeoutS, s.idle_timeout_s);
    writeIfChanged(store, kKeyOrientation, static_cast<int32_t>(s.orientation));
    writeIfChanged(store, kKeyDemoMode, s.demo_mode ? 1 : 0);
    writeIfChanged(store, kKeyDebugLog, s.debug_log ? 1 : 0);
    std::string cur;
    if (!store.getStr(kKeyWifiProfile, cur) || cur != s.wifi_profile) store.setStr(kKeyWifiProfile, s.wifi_profile);
    writeIfChanged(store, kKeyHaLink, static_cast<int32_t>(s.ha_link));
    writeIfChanged(store, kKeyKeepAwake, static_cast<int32_t>(s.keep_awake));
    writeIfChanged(store, kKeyLearnBattery, s.learn_battery ? 1 : 0);
}

}  // namespace settings
