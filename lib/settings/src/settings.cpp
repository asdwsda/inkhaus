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

bool isValidWakeMode(int32_t v) {
    return v == static_cast<int32_t>(WakeMode::Button) || v == static_cast<int32_t>(WakeMode::Touch);
}
bool isValidRefreshIntervalMin(int32_t v) { return contains(kRefreshIntervalMinValues, v); }
bool isValidIdleTimeoutS(int32_t v) { return contains(kIdleTimeoutSValues, v); }

bool isValidOrientation(int32_t v) {
    return v == static_cast<int32_t>(Orientation::Portrait) ||
           v == static_cast<int32_t>(Orientation::Landscape);
}

Settings load(const KeyValueStore& store) {
    Settings s;
    int32_t v;
    if (store.getInt(kKeySchemaVersion, v) && v != kSchemaVersion) return s;
    if (store.getInt(kKeyWakeMode, v) && isValidWakeMode(v)) s.wake_mode = static_cast<WakeMode>(v);
    if (store.getInt(kKeyRefreshMin, v) && isValidRefreshIntervalMin(v)) s.refresh_interval_min = v;
    if (store.getInt(kKeyIdleTimeoutS, v) && isValidIdleTimeoutS(v)) s.idle_timeout_s = v;
    if (store.getInt(kKeyOrientation, v) && isValidOrientation(v))
        s.orientation = static_cast<Orientation>(v);
    return s;
}

void save(KeyValueStore& store, const Settings& s) {
    writeIfChanged(store, kKeySchemaVersion, kSchemaVersion);
    writeIfChanged(store, kKeyWakeMode, static_cast<int32_t>(s.wake_mode));
    writeIfChanged(store, kKeyRefreshMin, s.refresh_interval_min);
    writeIfChanged(store, kKeyIdleTimeoutS, s.idle_timeout_s);
    writeIfChanged(store, kKeyOrientation, static_cast<int32_t>(s.orientation));
}

}  // namespace settings
