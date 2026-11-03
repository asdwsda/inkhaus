#pragma once
// User settings (stored in NVS on device). Pure logic, no Arduino includes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace settings {

// Sleep mode (task 085), in cost order, cheapest first. PowerOff: main rail off, the rocker (or the RTC alarm)
// switches the device on. DeepSleep: ESP32 deep sleep, rocker wakes. Touch: a tap wakes. TouchAndRocker: both.
enum class SleepSetting : int32_t { PowerOff = 0, DeepSleep = 1, Touch = 2, TouchAndRocker = 3 };
// Keep awake (task 085): never idle-sleep. OnUsb only while USB power is detected (see keepAwakeOnUsbOffered).
enum class KeepAwake : int32_t { Off = 0, OnUsb = 1, Always = 2 };

// What this hardware/firmware can do. Verified by task 084's battery test: power-off works, the BM8563 alarm
// powers the device on again from power-off, and the CH9102 RX probe (GPIO3, task 081) reads 0 on every battery
// wake and 1 on every USB wake, so "On USB" is offered in every sleep mode.
struct Capabilities {
    bool power_off = true;         // Power off sleep mode usable (migration of the old Button wake mode)
    bool rtc_power_on = true;      // the RTC alarm switches the device on from power-off
    bool usb_probe_reliable = true;
};
constexpr Capabilities kCapabilities{};
enum class Orientation : int32_t { Portrait = 0, Landscape = 1 };

// Which HA URL to use (task 047). Auto = whatever the connected Wi-Fi profile says.
enum class HaLink : int32_t { Auto = 0, Internal = 1, External = 2 };
constexpr int32_t kMaxWifiProfileIndex = 4;  // matches config::kMaxWifiProfiles (legacy index validation)

struct Settings {
    SleepSetting sleep_mode = SleepSetting::DeepSleep;
    int32_t refresh_interval_min = 60;  // updates while asleep; 0 = off
    int32_t idle_timeout_s = 20;
    Orientation orientation = Orientation::Portrait;
    bool demo_mode = false;  // built-in demo showcase instead of config.yaml + Home Assistant
    bool debug_log = false;  // SD wake log + /debug.log (task 041)
    // Forced Wi-Fi profile by NAME (stable when the profile list is reordered); "" = Auto (first known
    // network in range). A name that is not in the list counts as Auto.
    std::string wifi_profile;
    // Set by load() when only the old index key "wifiprof" (1-based) exists; migrateWifiProfile() turns it
    // into a name. Not part of equality.
    int32_t legacy_wifi_index = 0;
    HaLink ha_link = HaLink::Auto;
    KeepAwake keep_awake = KeepAwake::Off;
    bool learn_battery = true;    // task 079: learn the low-battery limit from brownouts
    bool report_ha = true;        // task 100: post battery/temperature/humidity/last wake to HA (default On, user 2026-11-03)

    bool operator==(const Settings& o) const {
        return sleep_mode == o.sleep_mode && refresh_interval_min == o.refresh_interval_min &&
               idle_timeout_s == o.idle_timeout_s && orientation == o.orientation &&
               demo_mode == o.demo_mode && debug_log == o.debug_log &&
               wifi_profile == o.wifi_profile && ha_link == o.ha_link &&
               keep_awake == o.keep_awake && learn_battery == o.learn_battery && report_ha == o.report_ha;
    }
    bool operator!=(const Settings& o) const { return !(*this == o); }
};

// Allowed values in display order (the settings screen cycles through them).
constexpr std::array<SleepSetting, 4> kSleepModeValues = {SleepSetting::PowerOff, SleepSetting::DeepSleep, SleepSetting::Touch,
                                                       SleepSetting::TouchAndRocker};
constexpr std::array<KeepAwake, 3> kKeepAwakeValues = {KeepAwake::Off, KeepAwake::OnUsb, KeepAwake::Always};
constexpr std::array<Orientation, 2> kOrientationValues = {Orientation::Portrait,
                                                           Orientation::Landscape};
constexpr std::array<int32_t, 6> kRefreshIntervalMinValues = {0, 30, 60, 180, 360, 720};
constexpr std::array<int32_t, 4> kIdleTimeoutSValues = {10, 20, 30, 60};

// 3: sleep_mode + keepawake replace wake_mode + stayusb; intervals 0/30/60/180/360/720 (task 085).
// 2: wake_mode 1 = Touch only, 2 = Touch & Button (1: 1 = both).
constexpr int32_t kSchemaVersion = 3;

// Storage keys (NVS limits keys to 15 chars).
constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyWakeMode = "wake_mode";  // schema 1/2 only (read for migration)
constexpr const char* kKeySleepMode = "sleep_mode";  // schema 3
constexpr const char* kKeyRefreshMin = "refresh_min";
constexpr const char* kKeyIdleTimeoutS = "idle_s";
constexpr const char* kKeyOrientation = "orientation";
constexpr const char* kKeyDemoMode = "demo";  // 0 / 1; absent in old stores = off
constexpr const char* kKeyDebugLog = "dbglog";  // 0 / 1; absent = off (no schema bump)
constexpr const char* kKeyWifiProfileLegacy = "wifiprof";  // old: 1-based index, 0 = Auto (read once, migrated)
constexpr const char* kKeyWifiProfile = "wifiname";        // profile name (string, "" = Auto); absent = Auto
constexpr const char* kKeyHaLink = "halink";         // HaLink; absent = Auto
constexpr const char* kKeyStayAwakeUsb = "stayusb";  // old 0 / 1 (read for migration to keep_awake)
constexpr const char* kKeyKeepAwake = "keepawake";   // KeepAwake; absent = Off
constexpr const char* kKeyLearnBattery = "learnbat";  // 0 / 1; absent = on (no schema bump)
constexpr const char* kKeyReportHa = "reportha";     // 0 / 1; absent = on (task 100, no schema bump)

// Abstract key-value store of ints.
class KeyValueStore {
public:
    virtual ~KeyValueStore() = default;
    // Returns true and sets out if the key exists.
    virtual bool getInt(const char* key, int32_t& out) const = 0;
    virtual void setInt(const char* key, int32_t value) = 0;
    virtual bool getStr(const char* key, std::string& out) const = 0;
    virtual void setStr(const char* key, const std::string& value) = 0;
};

bool isValidSleepMode(int32_t v);
bool isValidKeepAwake(int32_t v);
bool isValidRefreshIntervalMin(int32_t v);
bool isValidIdleTimeoutS(int32_t v);
bool isValidOrientation(int32_t v);
bool isValidDemoMode(int32_t v);
bool isValidDebugLog(int32_t v);
bool isValidWifiProfileIndex(int32_t v);  // legacy index: 0..kMaxWifiProfileIndex
bool isValidHaLink(int32_t v);
bool isValidLearnBattery(int32_t v);
bool isValidReportHa(int32_t v);

// Nearest allowed update interval (old values 15/120/240 from schema 2); ties go to the longer one.
int32_t snapRefreshIntervalMin(int32_t v);

// ---- rules between the sleep fields (task 085, pure) ----
// On USB is offered where USB is detected without extra awake time: in Power off (via the power-off fallback).
bool keepAwakeOnUsbOffered(SleepSetting m, const Capabilities& c = kCapabilities);
// Power off without a working RTC power-on cannot wake for updates: they are forced Off (the stored value stays).
bool updatesForcedOff(const Settings& s, const Capabilities& c = kCapabilities);
int32_t effectiveRefreshMin(const Settings& s, const Capabilities& c = kCapabilities);
// OnUsb in a mode that does not offer it counts as Off.
KeepAwake effectiveKeepAwake(const Settings& s, const Capabilities& c = kCapabilities);
// Edit-buffer rule: a sleep mode that no longer offers On USB turns the buffer value into Off.
Settings normalizeSleepFields(Settings s, const Capabilities& c = kCapabilities);

// 1-based position of `name` in `names`, 0 (Auto) for "" or an unknown name.
int wifiProfileIndex(const std::string& name, const std::vector<std::string>& names);

// One-time migration of the old index key: when load() found only the legacy index, resolve it against the
// profile list. Returns true when settings changed or must be saved (the name key then exists, so the legacy
// key is never read again). An index beyond the list becomes Auto.
bool migrateWifiProfile(Settings& s, const std::vector<std::string>& names);

// Missing/invalid values fall back to the default per field. An unknown
// schema_version yields all defaults.
Settings load(const KeyValueStore& store);

// Writes only fields (and schema_version) whose stored value differs or is missing.
void save(KeyValueStore& store, const Settings& s);

}  // namespace settings
