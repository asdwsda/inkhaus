#include <map>
#include <string>
#include <unity.h>

#include "battery_learn.h"
#include "power.h"

using namespace power;

namespace {
struct MemStore : settings::KeyValueStore {
    std::map<std::string, int32_t> d;
    bool getInt(const char* k, int32_t& o) const override {
        auto it = d.find(k);
        if (it == d.end()) return false;
        o = it->second;
        return true;
    }
    void setInt(const char* k, int32_t v) override { d[k] = v; }
    bool getStr(const char*, std::string&) const override { return false; }
    void setStr(const char*, const std::string&) override {}
};
// One full charge cycle: a stored discharge below 3900, then >= 4150 on two readings 10 min apart.
void cycle(BatteryLearn& st) {
    st.last_mv = 3800;
    on_sample(st, 4200, 1000);
    on_sample(st, 4200, 1600);
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_brownout_gives_limit_plus_margin() {
    BatteryLearn st;
    TEST_ASSERT_EQUAL_INT32(3300, effective_limit(st, true));
    record_event(st, 3780);
    TEST_ASSERT_EQUAL_INT32(3830, effective_limit(st, true));
    TEST_ASSERT_TRUE(is_learned(st, true));
}

void test_mean_of_recent_events() {
    BatteryLearn st;
    record_event(st, 3700);
    record_event(st, 3750);
    record_event(st, 3800);
    TEST_ASSERT_EQUAL_INT32(3800, effective_limit(st, true));  // mean 3750 + 50
    cycle(st);
    record_event(st, 3600);  // cycles 0 and 1 both count: (3700+3750+3800+3600)/4 = 3712
    TEST_ASSERT_EQUAL_INT32(3762, effective_limit(st, true));
    cycle(st);
    record_event(st, 3500);  // cycle 0 dropped: (3600+3500)/2 = 3550
    TEST_ASSERT_EQUAL_INT32(3600, effective_limit(st, true));
}

void test_replay_one_discharge() {
    BatteryLearn st;
    const int32_t ev[] = {3924, 3902, 3838, 3800, 3770, 3726};
    const int32_t want[] = {3900, 3900, 3900, 3900, 3896, 3876};
    for (int i = 0; i < 6; ++i) {
        record_event(st, ev[i]);
        TEST_ASSERT_EQUAL_INT32(want[i], effective_limit(st, true));
    }
    TEST_ASSERT_TRUE(effective_limit(st, true) < 3900);
}

void test_bounds() {
    BatteryLearn st;
    record_event(st, 3990);
    TEST_ASSERT_EQUAL_INT32(3900, effective_limit(st, true));
    BatteryLearn lo;
    record_event(lo, 2900);
    TEST_ASSERT_EQUAL_INT32(3300, effective_limit(lo, true));
    record_event(lo, 0);  // ignored
    TEST_ASSERT_EQUAL_INT(1, lo.n_events);
    BatteryLearn bad;
    bad.limit_mv = 5000;
    TEST_ASSERT_EQUAL_INT32(3900, effective_limit(bad, true));
}

void test_lower_event_lowers_limit() {
    BatteryLearn st;
    record_event(st, 3800);
    record_event(st, 3500);  // the limit follows the mean: it can learn downward
    TEST_ASSERT_EQUAL_INT32(3700, effective_limit(st, true));
}

void test_slow_decay_per_eventless_cycle() {
    BatteryLearn st;
    record_event(st, 3700);  // 3750
    cycle(st);               // the cycle with the event: no decay
    TEST_ASSERT_EQUAL_INT32(3750, effective_limit(st, true));
    cycle(st);
    TEST_ASSERT_EQUAL_INT32(3700, effective_limit(st, true));
    cycle(st);
    TEST_ASSERT_EQUAL_INT32(3650, effective_limit(st, true));
    for (int i = 0; i < 50; ++i) cycle(st);
    TEST_ASSERT_EQUAL_INT32(3300, effective_limit(st, true));  // floor
}

void test_cycle_needs_discharge_and_two_full_readings() {
    BatteryLearn st;
    record_event(st, 3700);
    on_sample(st, 4200, 0);
    on_sample(st, 4200, 600);  // never discharged (no stored last_mv): not a cycle
    TEST_ASSERT_EQUAL_INT32(0, st.cycle);
    st.last_mv = 3850;
    on_sample(st, 4000, 1100);  // clock restarts
    TEST_ASSERT_FALSE(on_sample(st, 4160, 1200));  // first full reading
    TEST_ASSERT_FALSE(on_sample(st, 4160, 1499));  // < 5 min
    TEST_ASSERT_TRUE(on_sample(st, 4160, 1500));
    TEST_ASSERT_EQUAL_INT32(1, st.cycle);
    TEST_ASSERT_EQUAL_INT32(0, st.last_mv);
}

void test_short_usb_plugin_does_not_end_cycle() {
    BatteryLearn st;
    st.last_mv = 3800;
    st.limit_mv = 3800;
    TEST_ASSERT_FALSE(on_sample(st, 4180, 1000));  // plug in
    TEST_ASSERT_FALSE(on_sample(st, 4190, 1120));  // 2 min later
    TEST_ASSERT_FALSE(on_sample(st, 3950, 1130));  // unplugged, battery settles
    TEST_ASSERT_FALSE(on_sample(st, 4180, 1400));  // plugged again: clock restarts
    TEST_ASSERT_FALSE(on_sample(st, 4180, 1500));
    TEST_ASSERT_EQUAL_INT32(0, st.cycle);
    TEST_ASSERT_EQUAL_INT32(3800, st.limit_mv);
    TEST_ASSERT_EQUAL_INT32(3800, st.last_mv);
}

void test_learning_off_gives_default_and_keeps_values() {
    BatteryLearn st;
    record_event(st, 3780);
    TEST_ASSERT_EQUAL_INT32(3300, effective_limit(st, false));
    TEST_ASSERT_FALSE(is_learned(st, false));
    TEST_ASSERT_EQUAL_INT32(3830, effective_limit(st, true));  // restored
}

void test_event_ring_keeps_last_8() {
    BatteryLearn st;
    for (int i = 0; i < 12; ++i) record_event(st, 3400 + i);
    TEST_ASSERT_EQUAL_INT(8, st.n_events);
    TEST_ASSERT_EQUAL_INT32(3404, st.events[0].mv);
}

void test_event_classification() {
    TEST_ASSERT_TRUE(is_battery_event(ResetKind::Brownout, 3780, 3700));
    TEST_ASSERT_FALSE(is_battery_event(ResetKind::Brownout, 0, 3700));
    TEST_ASSERT_TRUE(is_battery_event(ResetKind::Crash, 3900, 3880));
    TEST_ASSERT_TRUE(is_battery_event(ResetKind::Crash, 3800, 3830));   // +30 allowed
    TEST_ASSERT_FALSE(is_battery_event(ResetKind::Crash, 3800, 3831));  // boot reading higher: USB
    TEST_ASSERT_FALSE(is_battery_event(ResetKind::Crash, 4100, 4100));
    TEST_ASSERT_FALSE(is_battery_event(ResetKind::Other, 3500, 3500));
}

void test_last_mv_store_rule() {
    TEST_ASSERT_FALSE(should_store_last_mv(0, 4100));     // >= 4000
    TEST_ASSERT_TRUE(should_store_last_mv(0, 3990));      // nothing stored
    TEST_ASSERT_FALSE(should_store_last_mv(3990, 3975));  // drop < 20
    TEST_ASSERT_TRUE(should_store_last_mv(3990, 3970));
    TEST_ASSERT_FALSE(should_store_last_mv(3800, 3900));  // rose
    TEST_ASSERT_EQUAL_INT32(3700, pick_last_mv(true, 3700, 3800));
    TEST_ASSERT_EQUAL_INT32(3800, pick_last_mv(false, 3700, 3800));
    TEST_ASSERT_EQUAL_INT32(3800, pick_last_mv(true, 0, 3800));
}

void test_recharge_clears_stored_last_mv() {
    BatteryLearn st;
    cycle(st);
    TEST_ASSERT_EQUAL_INT32(0, st.last_mv);
}

void test_percent_scaled() {
    TEST_ASSERT_EQUAL_INT(battery_percent(3700), battery_percent_scaled(3700, 3300));
    TEST_ASSERT_EQUAL_INT(0, battery_percent_scaled(3800, 3800));
    TEST_ASSERT_EQUAL_INT(0, battery_percent_scaled(3700, 3800));
    TEST_ASSERT_EQUAL_INT(100, battery_percent_scaled(4200, 3800));
    int prev = -1;
    for (int mv = 3800; mv <= 4200; mv += 10) {
        const int p = battery_percent_scaled(mv, 3800);
        TEST_ASSERT_TRUE(p >= prev);
        prev = p;
    }
}

void test_persistence_roundtrip() {
    BatteryLearn st;
    record_event(st, 3780);
    record_event(st, 3746);
    st.cycle = 4;
    st.last_mv = 3790;
    MemStore m;
    save_learn(m, st);
    BatteryLearn r = load_learn(m);
    TEST_ASSERT_TRUE(st == r);
    TEST_ASSERT_EQUAL_INT32(3813, effective_limit(r, true));  // (3780+3746)/2 + 50
}

void test_persistence_ignores_garbage() {
    MemStore m;
    m.d["lim"] = 9999;
    m.d["nev"] = 99;
    m.d["em0"] = 10;
    BatteryLearn r = load_learn(m);
    TEST_ASSERT_EQUAL_INT32(0, r.limit_mv);
    TEST_ASSERT_EQUAL_INT(0, r.n_events);
}

void test_decide_session_uses_limit() {
    TEST_ASSERT_TRUE(decide_session(WakeReason::Button, 3800, 3880) == Session::LowBattery);
    TEST_ASSERT_TRUE(decide_session(WakeReason::Button, 3800) == Session::Interactive);
    TEST_ASSERT_TRUE(below_low_battery(3799, 3800));
    TEST_ASSERT_FALSE(below_low_battery(3800, 3800));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_brownout_gives_limit_plus_margin);
    RUN_TEST(test_mean_of_recent_events);
    RUN_TEST(test_replay_one_discharge);
    RUN_TEST(test_bounds);
    RUN_TEST(test_lower_event_lowers_limit);
    RUN_TEST(test_slow_decay_per_eventless_cycle);
    RUN_TEST(test_cycle_needs_discharge_and_two_full_readings);
    RUN_TEST(test_short_usb_plugin_does_not_end_cycle);
    RUN_TEST(test_learning_off_gives_default_and_keeps_values);
    RUN_TEST(test_event_ring_keeps_last_8);
    RUN_TEST(test_event_classification);
    RUN_TEST(test_last_mv_store_rule);
    RUN_TEST(test_recharge_clears_stored_last_mv);
    RUN_TEST(test_percent_scaled);
    RUN_TEST(test_persistence_roundtrip);
    RUN_TEST(test_persistence_ignores_garbage);
    RUN_TEST(test_decide_session_uses_limit);
    return UNITY_END();
}
