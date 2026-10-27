#pragma once
#include "settings.h"

#include <Preferences.h>

// NVS backend (namespace "settings") of the settings key-value interface.
class NvsSettingsStore : public settings::KeyValueStore {
public:
    bool begin();  // opens the namespace; false on failure
    void end();
    bool getInt(const char* key, int32_t& out) const override;
    void setInt(const char* key, int32_t value) override;

private:
    mutable Preferences prefs_;
    bool open_ = false;
};
