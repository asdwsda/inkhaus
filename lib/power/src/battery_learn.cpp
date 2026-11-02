#include "battery_learn.h"

#include <algorithm>
#include <cstdio>

#include "power.h"

namespace power {

namespace {
int32_t clampLimit(int32_t v) { return std::min(kMaxLimitMv, std::max(kMinLimitMv, v)); }

int32_t recentMean(const BatteryLearn& st) {
    int64_t sum = 0;
    int n = 0;
    for (int i = 0; i < st.n_events; ++i)
        if (st.events[i].cycle > st.cycle - kRecentCycles) {
            sum += st.events[i].mv;
            ++n;
        }
    return n ? int32_t(sum / n) : 0;
}

void key(char* b, size_t n, const char* p, int i) { std::snprintf(b, n, "%s%d", p, i); }

void putIfChanged(settings::KeyValueStore& s, const char* k, int32_t v) {
    int32_t cur = 0;
    if (!s.getInt(k, cur) || cur != v) s.setInt(k, v);
}
}  // namespace

bool BatteryLearn::operator==(const BatteryLearn& o) const {
    if (limit_mv != o.limit_mv || cycle != o.cycle || last_mv != o.last_mv ||
        n_events != o.n_events)
        return false;
    for (int i = 0; i < n_events; ++i)
        if (events[i].mv != o.events[i].mv || events[i].cycle != o.events[i].cycle) return false;
    return true;
}

int32_t effective_limit(const BatteryLearn& st, bool learning_on) {
    if (!learning_on || st.limit_mv <= 0) return kMinLimitMv;
    return clampLimit(st.limit_mv);
}

bool is_battery_event(ResetKind k, int32_t last_mv, int32_t boot_mv) {
    if (k == ResetKind::Brownout) return last_mv > 0;
    if (k == ResetKind::Crash)
        return last_mv > 0 && last_mv < kCrashMaxMv && (boot_mv <= 0 || boot_mv <= last_mv + kCrashSlackMv);
    return false;
}

void record_event(BatteryLearn& st, int32_t mv) {
    if (mv <= 0) return;
    if (st.n_events == kMaxEvents) {
        for (int i = 1; i < kMaxEvents; ++i) st.events[i - 1] = st.events[i];
        --st.n_events;
    }
    st.events[st.n_events++] = {mv, st.cycle};
    st.limit_mv = clampLimit(recentMean(st) + kMarginMv);
}

bool on_sample(BatteryLearn& st, int32_t mv, int32_t now_s) {
    if (mv <= 0) return false;
    if (mv < kFullMv) {
        st.full_since_s = -1;
        return false;
    }
    if (st.full_since_s < 0 || now_s < st.full_since_s) {
        st.full_since_s = now_s;
        return false;
    }
    if (now_s - st.full_since_s < kFullHoldS) return false;
    if (st.last_mv <= 0 || st.last_mv >= kBelowMv) return false;  // no real discharge since the last cycle end
    bool hadEvent = false;
    for (int i = 0; i < st.n_events; ++i) hadEvent = hadEvent || st.events[i].cycle == st.cycle;
    if (!hadEvent && st.limit_mv > 0) st.limit_mv = std::max(kMinLimitMv, st.limit_mv - kDecayMv);
    ++st.cycle;
    st.last_mv = 0;
    st.full_since_s = -1;
    return true;
}

bool should_store_last_mv(int32_t stored_mv, int32_t mv) {
    if (mv <= 0 || mv >= kStoreBelowMv) return false;
    return stored_mv <= 0 || stored_mv - mv >= kStoreDropMv;
}

int32_t pick_last_mv(bool rtc_valid, int32_t rtc_mv, int32_t nvs_mv) {
    return rtc_valid && rtc_mv > 0 ? rtc_mv : nvs_mv;
}

int battery_percent_scaled(int32_t mv, int32_t empty_mv) {
    if (empty_mv <= kMinLimitMv) return battery_percent(mv);
    constexpr int32_t kFull = 4200;
    if (mv <= empty_mv) return 0;
    if (mv >= kFull) return 100;
    const int64_t num = int64_t(mv - empty_mv) * (kFull - kMinLimitMv);
    return battery_percent(kMinLimitMv + int32_t(num / (kFull - empty_mv)));
}

BatteryLearn load_learn(const settings::KeyValueStore& store) {
    BatteryLearn st;
    int32_t v = 0;
    if (store.getInt("lim", v) && v >= kMinLimitMv && v <= kMaxLimitMv) st.limit_mv = v;
    if (store.getInt("cyc", v) && v >= 0) st.cycle = v;
    if (store.getInt("lmv", v) && v >= 0 && v < 5000) st.last_mv = v;
    int32_t n = 0;
    if (store.getInt("nev", n) && n > 0) {
        for (int i = 0; i < std::min<int32_t>(n, kMaxEvents); ++i) {
            char k[8];
            int32_t mv = 0, cy = 0;
            key(k, sizeof k, "em", i);
            if (!store.getInt(k, mv) || mv < 2500 || mv > 4500) continue;
            key(k, sizeof k, "ec", i);
            if (!store.getInt(k, cy)) cy = st.cycle;
            st.events[st.n_events++] = {mv, cy};
        }
    }
    return st;
}

void save_learn(settings::KeyValueStore& store, const BatteryLearn& st) {
    putIfChanged(store, "lim", st.limit_mv);
    putIfChanged(store, "cyc", st.cycle);
    putIfChanged(store, "lmv", st.last_mv);
    putIfChanged(store, "nev", st.n_events);
    for (int i = 0; i < st.n_events; ++i) {
        char k[8];
        key(k, sizeof k, "em", i);
        putIfChanged(store, k, st.events[i].mv);
        key(k, sizeof k, "ec", i);
        putIfChanged(store, k, st.events[i].cycle);
    }
}

}  // namespace power
