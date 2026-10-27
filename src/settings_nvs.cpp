#include "settings_nvs.h"

bool NvsSettingsStore::begin() {
    open_ = prefs_.begin("settings", false);
    return open_;
}

void NvsSettingsStore::end() {
    if (open_) prefs_.end();
    open_ = false;
}

bool NvsSettingsStore::getInt(const char* key, int32_t& out) const {
    if (!open_ || !prefs_.isKey(key)) return false;
    out = prefs_.getInt(key, 0);
    return true;
}

void NvsSettingsStore::setInt(const char* key, int32_t value) {
    if (open_) prefs_.putInt(key, value);
}
