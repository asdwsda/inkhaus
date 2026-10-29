#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <unity.h>

#include "settings.h"

using namespace settings;

namespace {
struct FakeStore : KeyValueStore {
    std::map<std::string, int32_t> data;
    std::map<std::string, std::string> strs;
    int writes = 0;
    bool getStr(const char* key, std::string& out) const override {
        auto it = strs.find(key);
        if (it == strs.end()) return false;
        out = it->second;
        return true;
    }
    void setStr(const char* key, const std::string& value) override {
        strs[key] = value;
        ++writes;
    }
    bool getInt(const char* key, int32_t& out) const override {
        auto it = data.find(key);
        if (it == data.end()) return false;
        out = it->second;
        return true;
    }
    void setInt(const char* key, int32_t value) override {
        data[key] = value;
        ++writes;
    }
};
}  // namespace

void test_debug_log_default_old_store_invalid_roundtrip() {
    FakeStore st;
    TEST_ASSERT_FALSE(load(st).debug_log);
    st.data[kKeySchemaVersion] = kSchemaVersion;  // an old store without "dbglog"
    TEST_ASSERT_FALSE(load(st).debug_log);
    st.data[kKeyDebugLog] = 7;
    TEST_ASSERT_FALSE(load(st).debug_log);
    Settings s;
    s.debug_log = true;
    save(st, s);
    TEST_ASSERT_EQUAL_INT32(1, st.data[kKeyDebugLog]);
    TEST_ASSERT_TRUE(load(st).debug_log);
    TEST_ASSERT_TRUE(s != Settings());
}

void test_demo_mode_default_old_store_invalid_roundtrip() {
    FakeStore st;
    TEST_ASSERT_FALSE(load(st).demo_mode);
    st.data[kKeySchemaVersion] = kSchemaVersion;
    st.data[kKeyWakeMode] = 1;  // an old store without "demo"
    TEST_ASSERT_FALSE(load(st).demo_mode);
    TEST_ASSERT_EQUAL_INT(1, (int)load(st).wake_mode);
    st.data[kKeyDemoMode] = 2;
    TEST_ASSERT_FALSE(load(st).demo_mode);
    st.data[kKeyDemoMode] = -1;
    TEST_ASSERT_FALSE(load(st).demo_mode);
    Settings s;
    s.demo_mode = true;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(1, st.data[kKeyDemoMode]);
    TEST_ASSERT_TRUE(load(st).demo_mode);
    TEST_ASSERT_TRUE(s != Settings());
    TEST_ASSERT_TRUE(std::strlen(kKeyDemoMode) <= 15);
    int w = st.writes;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(w, st.writes);
}

void setUp() {}
void tearDown() {}

void test_defaults_with_empty_store() {
    FakeStore st;
    Settings s = load(st);
    TEST_ASSERT_TRUE(s.wake_mode == WakeMode::Button);
    TEST_ASSERT_EQUAL_INT32(60, s.refresh_interval_min);
    TEST_ASSERT_EQUAL_INT32(20, s.idle_timeout_s);
    TEST_ASSERT_TRUE(s.orientation == Orientation::Portrait);
}

void test_orientation_default_on_pre_orientation_store() {
    FakeStore st;
    st.data[kKeySchemaVersion] = kSchemaVersion;
    st.data[kKeyWakeMode] = 1;
    st.data[kKeyRefreshMin] = 15;
    st.data[kKeyIdleTimeoutS] = 10;
    Settings s = load(st);
    TEST_ASSERT_TRUE(s.orientation == Orientation::Portrait);
    TEST_ASSERT_TRUE(s.wake_mode == WakeMode::Touch);
    TEST_ASSERT_EQUAL_INT32(15, s.refresh_interval_min);
    TEST_ASSERT_EQUAL_INT32(10, s.idle_timeout_s);
}

void test_orientation_invalid_defaults_only_that_field() {
    const int32_t bad[] = {-1, 2, 99, INT32_MIN, INT32_MAX};
    for (int32_t b : bad) {
        FakeStore st;
        st.data[kKeyWakeMode] = 1;
        st.data[kKeyRefreshMin] = 15;
        st.data[kKeyIdleTimeoutS] = 10;
        st.data[kKeyOrientation] = b;
        Settings s = load(st);
        TEST_ASSERT_TRUE(s.orientation == Orientation::Portrait);
        TEST_ASSERT_TRUE(s.wake_mode == WakeMode::TouchAndButton);
        TEST_ASSERT_EQUAL_INT32(15, s.refresh_interval_min);
        TEST_ASSERT_EQUAL_INT32(10, s.idle_timeout_s);
    }
    FakeStore st;
    st.data[kKeyOrientation] = 1;
    TEST_ASSERT_TRUE(load(st).orientation == Orientation::Landscape);
}

void test_invalid_value_defaults_only_that_field() {
    FakeStore st;
    st.data[kKeyRefreshMin] = 7;
    st.data[kKeyIdleTimeoutS] = 30;
    st.data[kKeyWakeMode] = 1;
    Settings s = load(st);
    TEST_ASSERT_EQUAL_INT32(60, s.refresh_interval_min);
    TEST_ASSERT_EQUAL_INT32(30, s.idle_timeout_s);
    TEST_ASSERT_TRUE(s.wake_mode == WakeMode::TouchAndButton);

    st.data[kKeyWakeMode] = 99;
    TEST_ASSERT_TRUE(load(st).wake_mode == WakeMode::Button);
}

void test_roundtrip_all_allowed_values() {
    for (WakeMode w : kWakeModeValues)
        for (int32_t r : kRefreshIntervalMinValues)
            for (int32_t i : kIdleTimeoutSValues)
              for (Orientation o : kOrientationValues) {
                FakeStore st;
                Settings in;
                in.wake_mode = w;
                in.refresh_interval_min = r;
                in.idle_timeout_s = i;
                in.orientation = o;
                save(st, in);
                TEST_ASSERT_TRUE(load(st) == in);
            }
}

void test_save_skips_unchanged_fields() {
    FakeStore st;
    Settings s;
    save(st, s);
    int first = st.writes;
    TEST_ASSERT_TRUE(first > 0);
    save(st, s);
    TEST_ASSERT_EQUAL_INT(first, st.writes);

    s.idle_timeout_s = 60;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(first + 1, st.writes);

    s.orientation = Orientation::Landscape;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(first + 2, st.writes);
    TEST_ASSERT_EQUAL_INT32(1, st.data[kKeyOrientation]);
}

void test_wake_mode_migration_and_roundtrip() {
    FakeStore st;
    st.data[kKeySchemaVersion] = 1;  // legacy: 1 = old Touch (touch and button)
    st.data[kKeyWakeMode] = 1;
    TEST_ASSERT_TRUE(load(st).wake_mode == WakeMode::TouchAndButton);
    st.data[kKeyWakeMode] = 0;
    TEST_ASSERT_TRUE(load(st).wake_mode == WakeMode::Button);
    st.data[kKeyWakeMode] = 2;  // not valid in schema 1 stores, but harmless
    FakeStore n;  // current schema: 1 = Touch only, 2 = Touch & Button
    n.data[kKeySchemaVersion] = kSchemaVersion;
    n.data[kKeyWakeMode] = 1;
    TEST_ASSERT_TRUE(load(n).wake_mode == WakeMode::Touch);
    n.data[kKeyWakeMode] = 2;
    TEST_ASSERT_TRUE(load(n).wake_mode == WakeMode::TouchAndButton);
    // After the migration is saved, the value 2 means Touch & Button and a reload keeps it.
    Settings m = load(st);
    FakeStore out;
    save(out, m);
    TEST_ASSERT_EQUAL_INT32(2, out.data[kKeyWakeMode]);
    TEST_ASSERT_TRUE(load(out).wake_mode == WakeMode::TouchAndButton);
    Settings t;
    t.wake_mode = WakeMode::Touch;
    save(out, t);
    TEST_ASSERT_TRUE(load(out).wake_mode == WakeMode::Touch);
    TEST_ASSERT_EQUAL_INT(3, (int)kWakeModeValues.size());
    TEST_ASSERT_FALSE(isValidWakeMode(3));
}

void test_unknown_schema_version_loads_defaults() {
    FakeStore st;
    Settings in;
    in.wake_mode = WakeMode::Touch;
    in.refresh_interval_min = 15;
    in.idle_timeout_s = 10;
    save(st, in);
    st.data[kKeySchemaVersion] = kSchemaVersion + 1;
    TEST_ASSERT_TRUE(load(st) == Settings());
}

void test_allowed_value_lists() {
    TEST_ASSERT_EQUAL_INT32(0, kRefreshIntervalMinValues.front());
    TEST_ASSERT_EQUAL_INT32(240, kRefreshIntervalMinValues.back());
    TEST_ASSERT_EQUAL_INT32(10, kIdleTimeoutSValues.front());
    TEST_ASSERT_EQUAL_INT32(60, kIdleTimeoutSValues.back());
}

void test_each_field_invalid_keeps_others() {
    const int32_t bad[] = {-1, 7, 1000, INT32_MIN, INT32_MAX};
    for (int32_t b : bad) {
        FakeStore st;
        st.data[kKeyWakeMode] = 1;
        st.data[kKeyRefreshMin] = 15;
        st.data[kKeyIdleTimeoutS] = 10;
        st.data[kKeyWakeMode] = b;
        Settings s = load(st);
        TEST_ASSERT_TRUE(s.wake_mode == WakeMode::Button);
        TEST_ASSERT_EQUAL_INT32(15, s.refresh_interval_min);
        TEST_ASSERT_EQUAL_INT32(10, s.idle_timeout_s);

        st.data[kKeyWakeMode] = 1;
        st.data[kKeyRefreshMin] = b;
        s = load(st);
        TEST_ASSERT_TRUE(s.wake_mode == WakeMode::TouchAndButton);
        TEST_ASSERT_EQUAL_INT32(60, s.refresh_interval_min);
        TEST_ASSERT_EQUAL_INT32(10, s.idle_timeout_s);

        st.data[kKeyRefreshMin] = 15;
        st.data[kKeyIdleTimeoutS] = b;
        s = load(st);
        TEST_ASSERT_TRUE(s.wake_mode == WakeMode::TouchAndButton);
        TEST_ASSERT_EQUAL_INT32(15, s.refresh_interval_min);
        TEST_ASSERT_EQUAL_INT32(20, s.idle_timeout_s);
    }
}

void test_values_valid_for_other_field_are_rejected() {
    // 0 is valid for refresh but not idle timeout; 10 is valid for idle but not refresh.
    FakeStore st;
    st.data[kKeyIdleTimeoutS] = 0;
    st.data[kKeyRefreshMin] = 10;
    Settings s = load(st);
    TEST_ASSERT_EQUAL_INT32(20, s.idle_timeout_s);
    TEST_ASSERT_EQUAL_INT32(60, s.refresh_interval_min);
}

void test_schema_version_variants() {
    const int32_t versions[] = {0, -1, kSchemaVersion + 1, INT32_MAX};
    for (int32_t v : versions) {
        FakeStore st;
        st.data[kKeyWakeMode] = 1;
        st.data[kKeyRefreshMin] = 15;
        st.data[kKeyIdleTimeoutS] = 10;
        st.data[kKeySchemaVersion] = v;
        TEST_ASSERT_TRUE(load(st) == Settings());
    }
    // Missing schema_version with valid fields: fields still load (no crash, no reset).
    FakeStore st;
    st.data[kKeyRefreshMin] = 15;
    TEST_ASSERT_EQUAL_INT32(15, load(st).refresh_interval_min);
}

void test_save_writes_only_changed_field_and_repairs_invalid() {
    FakeStore st;
    Settings s;
    save(st, s);
    st.writes = 0;
    s.refresh_interval_min = 240;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(1, st.writes);
    TEST_ASSERT_EQUAL_INT32(240, st.data[kKeyRefreshMin]);

    // Corrupt stored value gets overwritten by save of the default.
    st.data[kKeyIdleTimeoutS] = 7;
    st.writes = 0;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(1, st.writes);
    TEST_ASSERT_EQUAL_INT32(20, st.data[kKeyIdleTimeoutS]);
    TEST_ASSERT_TRUE(load(st) == s);

    st.writes = 0;
    s.orientation = Orientation::Landscape;
    save(st, s);
    TEST_ASSERT_EQUAL_INT(1, st.writes);
    TEST_ASSERT_TRUE(load(st) == s);
}

void test_save_into_future_schema_store_stamps_current_version() {
    FakeStore st;
    st.data[kKeySchemaVersion] = kSchemaVersion + 1;
    st.data[kKeyRefreshMin] = 15;
    Settings s;
    s.refresh_interval_min = 15;
    save(st, s);
    TEST_ASSERT_EQUAL_INT32(kSchemaVersion, st.data[kKeySchemaVersion]);
    TEST_ASSERT_TRUE(load(st) == s);
}

void test_keys_fit_nvs_limit_and_lists_are_valid() {
    TEST_ASSERT_TRUE(strlen(kKeySchemaVersion) <= 15);
    TEST_ASSERT_TRUE(strlen(kKeyWakeMode) <= 15);
    TEST_ASSERT_TRUE(strlen(kKeyRefreshMin) <= 15);
    TEST_ASSERT_TRUE(strlen(kKeyIdleTimeoutS) <= 15);
    TEST_ASSERT_TRUE(strlen(kKeyOrientation) <= 15);
    TEST_ASSERT_TRUE(kOrientationValues.front() == Orientation::Portrait);
    TEST_ASSERT_TRUE(kOrientationValues.back() == Orientation::Landscape);
    for (Orientation o : kOrientationValues)
        TEST_ASSERT_TRUE(isValidOrientation(static_cast<int32_t>(o)));
    TEST_ASSERT_FALSE(isValidOrientation(2));
    for (WakeMode w : kWakeModeValues) TEST_ASSERT_TRUE(isValidWakeMode(static_cast<int32_t>(w)));
    for (int32_t r : kRefreshIntervalMinValues) TEST_ASSERT_TRUE(isValidRefreshIntervalMin(r));
    for (int32_t i : kIdleTimeoutSValues) TEST_ASSERT_TRUE(isValidIdleTimeoutS(i));
    TEST_ASSERT_TRUE(isValidRefreshIntervalMin(Settings().refresh_interval_min));
    TEST_ASSERT_TRUE(isValidIdleTimeoutS(Settings().idle_timeout_s));
    TEST_ASSERT_FALSE(isValidRefreshIntervalMin(7));
    TEST_ASSERT_FALSE(isValidIdleTimeoutS(0));
}

void test_wifi_profile_by_name_and_ha_link() {
    FakeStore st;
    Settings d = load(st);  // old store without the keys
    TEST_ASSERT_TRUE(d.wifi_profile.empty());
    TEST_ASSERT_TRUE(d.ha_link == HaLink::Auto);
    st.data[kKeyHaLink] = 3;  // out of range
    d = load(st);
    TEST_ASSERT_TRUE(d.ha_link == HaLink::Auto);
    Settings s;
    s.wifi_profile = "Hotspot";
    s.ha_link = HaLink::External;
    save(st, s);
    TEST_ASSERT_EQUAL_STRING("Hotspot", st.strs[kKeyWifiProfile].c_str());
    TEST_ASSERT_EQUAL_INT32(2, st.data[kKeyHaLink]);
    Settings r = load(st);
    TEST_ASSERT_TRUE(r == s);
    const int w = st.writes;
    save(st, s);  // unchanged: no write
    TEST_ASSERT_EQUAL_INT(w, st.writes);
    s.ha_link = HaLink::Internal;
    TEST_ASSERT_FALSE(r == s);
    s = r;
    s.wifi_profile = "Home";
    TEST_ASSERT_FALSE(r == s);
    TEST_ASSERT_TRUE(strlen(kKeyWifiProfile) <= 15 && strlen(kKeyHaLink) <= 15 && strlen(kKeyWifiProfileLegacy) <= 15);
}

void test_wifi_profile_index_lookup() {
    const std::vector<std::string> names = {"Home", "Hotspot"};
    TEST_ASSERT_EQUAL_INT(0, wifiProfileIndex("", names));
    TEST_ASSERT_EQUAL_INT(1, wifiProfileIndex("Home", names));
    TEST_ASSERT_EQUAL_INT(2, wifiProfileIndex("Hotspot", names));
    TEST_ASSERT_EQUAL_INT(0, wifiProfileIndex("Gone", names));  // unknown = Auto
}

void test_wifi_profile_legacy_index_migrates_once() {
    const std::vector<std::string> names = {"Home", "Hotspot"};
    FakeStore st;
    st.data[kKeyWifiProfileLegacy] = 2;
    Settings s = load(st);
    TEST_ASSERT_EQUAL_INT32(2, s.legacy_wifi_index);
    TEST_ASSERT_TRUE(s.wifi_profile.empty());
    TEST_ASSERT_TRUE(migrateWifiProfile(s, names));
    TEST_ASSERT_EQUAL_STRING("Hotspot", s.wifi_profile.c_str());
    TEST_ASSERT_EQUAL_INT32(0, s.legacy_wifi_index);
    TEST_ASSERT_FALSE(migrateWifiProfile(s, names));  // only once
    st.strs[kKeyWifiProfile] = s.wifi_profile;       // what main stores
    // the name key wins from now on, the legacy index is not read
    Settings again = load(st);
    TEST_ASSERT_EQUAL_INT32(0, again.legacy_wifi_index);
    TEST_ASSERT_EQUAL_STRING("Hotspot", again.wifi_profile.c_str());
    // an index beyond the list becomes Auto; 0 and invalid values need no migration
    FakeStore st2;
    st2.data[kKeyWifiProfileLegacy] = 4;
    Settings b = load(st2);
    TEST_ASSERT_TRUE(migrateWifiProfile(b, names));
    TEST_ASSERT_TRUE(b.wifi_profile.empty());
    FakeStore st3;
    st3.data[kKeyWifiProfileLegacy] = 0;
    Settings c = load(st3);
    TEST_ASSERT_FALSE(migrateWifiProfile(c, names));
    st3.data[kKeyWifiProfileLegacy] = 9;
    TEST_ASSERT_EQUAL_INT32(0, load(st3).legacy_wifi_index);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_demo_mode_default_old_store_invalid_roundtrip);
    RUN_TEST(test_debug_log_default_old_store_invalid_roundtrip);
    RUN_TEST(test_defaults_with_empty_store);
    RUN_TEST(test_orientation_default_on_pre_orientation_store);
    RUN_TEST(test_orientation_invalid_defaults_only_that_field);
    RUN_TEST(test_invalid_value_defaults_only_that_field);
    RUN_TEST(test_roundtrip_all_allowed_values);
    RUN_TEST(test_save_skips_unchanged_fields);
    RUN_TEST(test_unknown_schema_version_loads_defaults);
    RUN_TEST(test_allowed_value_lists);
    RUN_TEST(test_each_field_invalid_keeps_others);
    RUN_TEST(test_values_valid_for_other_field_are_rejected);
    RUN_TEST(test_schema_version_variants);
    RUN_TEST(test_save_writes_only_changed_field_and_repairs_invalid);
    RUN_TEST(test_save_into_future_schema_store_stamps_current_version);
    RUN_TEST(test_keys_fit_nvs_limit_and_lists_are_valid);
    RUN_TEST(test_wake_mode_migration_and_roundtrip);
    RUN_TEST(test_wifi_profile_by_name_and_ha_link);
    RUN_TEST(test_wifi_profile_index_lookup);
    RUN_TEST(test_wifi_profile_legacy_index_migrates_once);
    return UNITY_END();
}
