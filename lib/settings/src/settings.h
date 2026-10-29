#pragma once
// User settings (stored in NVS on device). Pure logic, no Arduino includes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace settings {

// Button: rocker only; Touch: touch only; TouchAndButton: both (task 039). NVS schema 1 stored 1 = the old
// "Touch" (which woke on both); load() migrates it to TouchAndButton.
enum class WakeMode : int32_t { Button = 0, Touch = 1, TouchAndButton = 2 };
enum class Orientation : int32_t { Portrait = 0, Landscape = 1 };

// Which HA URL to use (task 047). Auto = whatever the connected Wi-Fi profile says.
enum class HaLink : int32_t { Auto = 0, Internal = 1, External = 2 };
constexpr int32_t kMaxWifiProfileIndex = 4;  // matches config::kMaxWifiProfiles (legacy index validation)

struct Settings {
    WakeMode wake_mode = WakeMode::Button;
    int32_t refresh_interval_min = 60;  // 0 = off
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

    bool operator==(const Settings& o) const {
        return wake_mode == o.wake_mode && refresh_interval_min == o.refresh_interval_min &&
               idle_timeout_s == o.idle_timeout_s && orientation == o.orientation &&
               demo_mode == o.demo_mode && debug_log == o.debug_log &&
               wifi_profile == o.wifi_profile && ha_link == o.ha_link;
    }
    bool operator!=(const Settings& o) const { return !(*this == o); }
};

// Allowed values in display order (the settings screen cycles through them).
constexpr std::array<WakeMode, 3> kWakeModeValues = {WakeMode::Button, WakeMode::Touch,
                                                         WakeMode::TouchAndButton};
constexpr std::array<Orientation, 2> kOrientationValues = {Orientation::Portrait,
                                                           Orientation::Landscape};
constexpr std::array<int32_t, 6> kRefreshIntervalMinValues = {0, 15, 30, 60, 120, 240};
constexpr std::array<int32_t, 4> kIdleTimeoutSValues = {10, 20, 30, 60};

constexpr int32_t kSchemaVersion = 2;  // 2: wake_mode 1 = Touch only, 2 = Touch & Button (1: 1 = both)

// Storage keys (NVS limits keys to 15 chars).
constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyWakeMode = "wake_mode";
constexpr const char* kKeyRefreshMin = "refresh_min";
constexpr const char* kKeyIdleTimeoutS = "idle_s";
constexpr const char* kKeyOrientation = "orientation";
constexpr const char* kKeyDemoMode = "demo";  // 0 / 1; absent in old stores = off
constexpr const char* kKeyDebugLog = "dbglog";  // 0 / 1; absent = off (no schema bump)
constexpr const char* kKeyWifiProfileLegacy = "wifiprof";  // old: 1-based index, 0 = Auto (read once, migrated)
constexpr const char* kKeyWifiProfile = "wifiname";        // profile name (string, "" = Auto); absent = Auto
constexpr const char* kKeyHaLink = "halink";         // HaLink; absent = Auto

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

bool isValidWakeMode(int32_t v);
bool isValidRefreshIntervalMin(int32_t v);
bool isValidIdleTimeoutS(int32_t v);
bool isValidOrientation(int32_t v);
bool isValidDemoMode(int32_t v);
bool isValidDebugLog(int32_t v);
bool isValidWifiProfileIndex(int32_t v);  // legacy index: 0..kMaxWifiProfileIndex
bool isValidHaLink(int32_t v);

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
