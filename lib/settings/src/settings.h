#pragma once
// User settings (stored in NVS on device). Pure logic, no Arduino includes.
#include <array>
#include <cstddef>
#include <cstdint>

namespace settings {

enum class WakeMode : int32_t { Button = 0, Touch = 1 };
enum class Orientation : int32_t { Portrait = 0, Landscape = 1 };

struct Settings {
    WakeMode wake_mode = WakeMode::Button;
    int32_t refresh_interval_min = 60;  // 0 = off
    int32_t idle_timeout_s = 20;
    Orientation orientation = Orientation::Portrait;
    bool demo_mode = false;  // built-in demo showcase instead of config.yaml + Home Assistant

    bool operator==(const Settings& o) const {
        return wake_mode == o.wake_mode && refresh_interval_min == o.refresh_interval_min &&
               idle_timeout_s == o.idle_timeout_s && orientation == o.orientation &&
               demo_mode == o.demo_mode;
    }
    bool operator!=(const Settings& o) const { return !(*this == o); }
};

// Allowed values in display order (the settings screen cycles through them).
constexpr std::array<WakeMode, 2> kWakeModeValues = {WakeMode::Button, WakeMode::Touch};
constexpr std::array<Orientation, 2> kOrientationValues = {Orientation::Portrait,
                                                           Orientation::Landscape};
constexpr std::array<int32_t, 6> kRefreshIntervalMinValues = {0, 15, 30, 60, 120, 240};
constexpr std::array<int32_t, 4> kIdleTimeoutSValues = {10, 20, 30, 60};

constexpr int32_t kSchemaVersion = 1;

// Storage keys (NVS limits keys to 15 chars).
constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyWakeMode = "wake_mode";
constexpr const char* kKeyRefreshMin = "refresh_min";
constexpr const char* kKeyIdleTimeoutS = "idle_s";
constexpr const char* kKeyOrientation = "orientation";
constexpr const char* kKeyDemoMode = "demo";  // 0 / 1; absent in old stores = off

// Abstract key-value store of ints.
class KeyValueStore {
public:
    virtual ~KeyValueStore() = default;
    // Returns true and sets out if the key exists.
    virtual bool getInt(const char* key, int32_t& out) const = 0;
    virtual void setInt(const char* key, int32_t value) = 0;
};

bool isValidWakeMode(int32_t v);
bool isValidRefreshIntervalMin(int32_t v);
bool isValidIdleTimeoutS(int32_t v);
bool isValidOrientation(int32_t v);
bool isValidDemoMode(int32_t v);

// Missing/invalid values fall back to the default per field. An unknown
// schema_version yields all defaults.
Settings load(const KeyValueStore& store);

// Writes only fields (and schema_version) whose stored value differs or is missing.
void save(KeyValueStore& store, const Settings& s);

}  // namespace settings
